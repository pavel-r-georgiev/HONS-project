/*
 * Copyright (c) 2014, University of Lugano
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holders nor the names of it
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <evpaxos.h>
#include <signal.h>
#include <pthread.h>
#include <event2/event-config.h>
#include <event2/thread.h>
#include <paxos_replica.h>
#include "include/time_rdtsc.h"
#include "include/utils.h"
#include "include/tpl.h"
#include "unistd.h"
#include "zlog.h"

struct timeval count_interval = {0, 0};
pthread_t thread_id;
struct fd_replica* replica_local;

struct args_struct{
    int id;
    struct fd_replica* replica;
};

// Used to stop main thread while initializing Paxos replica
int done = 0;
pthread_mutex_t paxos_initialization_mutex;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

// Mutex for Paxos listener state
pthread_mutex_t paxos_listener_mutex;
pthread_mutex_t detected_states_hashmap_lock;
//  Hashmap to hold Paxos messages UUID and time of sending the messages. Used to benchmark Paxos consensus time.
struct detected_state_struct *detected_states = NULL; /* important! initialize to NULL */

static void
handle_sigint(int sig, short ev, void* arg)
{
    struct event_base* base = arg;
    event_base_loopexit(base, NULL);
}

static void
init_state(struct fd_replica* replica)
{
    replica->instance_id = 0;
    replica->state =  malloc(sizeof(struct membership_state));
    replica->state->paxos_state_array = zlist_new();
    replica->state->paxos_received_state_array = zlist_new();
    replica->state->detected_state_array = zlist_new(); // only there for evaluation

//    Strings in the array will be freed automatically when list is destroyed
    zlist_autofree (replica->state->paxos_state_array);
    zlist_autofree(replica->state->paxos_received_state_array);
    zlist_autofree (replica->state->detected_state_array);
}

void clean_up_paxos_hashmap() {
    //    Clean up hashmap
    detected_state_struct *current_state, *tmp;
    if (pthread_mutex_lock(&detected_states_hashmap_lock) != 0){
        printf("ERROR: can't get Paxos hashmap mutex \n");
    }

    HASH_ITER(hh, detected_states, current_state, tmp) {
        HASH_DEL(detected_states, current_state);
        free(current_state->uuid);
        free(current_state);
    }

    pthread_mutex_unlock(&detected_states_hashmap_lock);
}


/**
 * Callback function of the learner in the replica learning a value
 * @param iid
 * @param value
 * @param size
 * @param arg
 */
static void
on_deliver(unsigned iid, char* value, size_t size, void* arg)
{
    struct fd_replica* replica = (struct fd_replica*)arg;
    char sender_ip[MAX_SIZE_IP_ADDRESS_STRING];
    byte uuid[ZUUID_LEN];
;
    if (pthread_mutex_lock(&paxos_listener_mutex) != 0) {
        printf("ERROR: can't get Paxos listener mutex \n");
        return;
    }
    zlist_purge(replica->state->paxos_received_state_array);
    if(deserialize_hash(value, size, replica->state->paxos_received_state_array, sender_ip, (char*) uuid) != 0){
        pthread_mutex_unlock(&paxos_listener_mutex);
        return;
    }


    //    Message sent from propser of paxos replica on same node
    if(strcmp(sender_ip, replica->current_node_ip) == 0){

        if (pthread_mutex_lock(&detected_states_hashmap_lock) != 0) {
            printf("ERROR: can't get mutex \n");
            return;
        }
//    Get UUID from message
        zuuid_t *uuid_t = zuuid_new ();
        zuuid_set(uuid_t, uuid);
//        printf("Received UUID: %s\n", zuuid_str(uuid_t));
//        print_string_list(replica->state->paxos_received_state_array);
//    Check if current node detected the state
        detected_state_struct *state = NULL;
        HASH_FIND_STR(detected_states, zuuid_str(uuid_t), state);
        zuuid_destroy (&uuid_t);

        if(state != NULL){
            zlog_category_t *c;
            c = zlog_get_category("paxos");
            zlog_info(c, "Paxos run UUID: %s | Reached consensus. Time from detection to consensus: %f ms", state->uuid, get_current_time_ms() - state->time_discovered);
            free(state->uuid);
            free(state);
            HASH_DEL(detected_states, state);
        }

        pthread_mutex_unlock(&detected_states_hashmap_lock);

        pthread_mutex_unlock(&paxos_listener_mutex);
        return;
    }

//    State is the same as current state
    if(is_equal_lists(replica->state->paxos_received_state_array, replica->state->paxos_state_array)){
        pthread_mutex_unlock(&paxos_listener_mutex);
        return;
    }

    copy_list(replica->state->paxos_received_state_array, replica->state->paxos_state_array);

    //    Get time passed since last logged state
    double time_now = get_current_time_ms();
    double time_passed = time_now - replica->state->last_logged_state_time_ms;
    replica->state->last_logged_state_time_ms = time_now;
//        print_string_list(replica->state->paxos_state_array);
    log_state_list(replica->state->paxos_state_array, time_passed);
//        printf("Value: %.64s, Size: %d \n", value, (int)size);
    replica->instance_id = iid;

    pthread_mutex_unlock(&paxos_listener_mutex);
}


void paxos_serialize_and_submit(
                struct fd_replica* replica,
                struct node_struct **nodes){
    if(nodes == NULL){
        printf("paxos_serialize_and_submit : NULL nodes structure \n");
        return;
    }

    // Init state struct - hold state buffer and length of buffer.
    struct membership_state *state = malloc(sizeof(struct membership_state));
    zuuid_t *uuid_t = zuuid_new ();
    byte uuid [ZUUID_LEN];
    zuuid_export (uuid_t, uuid);

//    Add detected state to hashmap
    if (pthread_mutex_lock(&detected_states_hashmap_lock) != 0) {
        printf("ERROR: can't get mutex \n");
        return;
    }

    detected_state_struct *detected_state = NULL;
    detected_state = malloc(sizeof(struct detected_state_struct));
    detected_state->uuid = malloc(ZUUID_STR_LEN);
    strcpy(detected_state->uuid, zuuid_str(uuid_t));
//    Note time of detection in order speed of receipt at Paxos learner
    detected_state->time_discovered = get_current_time_ms();
    HASH_ADD_STR(detected_states, uuid, detected_state);
    zuuid_destroy (&uuid_t);
    zlist_purge(replica->state->detected_state_array);
    get_membership_group_from_hash(nodes, replica->state->detected_state_array);

//    printf("Sending UUID: %s \n", detected_state->uuid);
//    print_string_list(replica->state->detected_state_array);

    zlog_category_t *c;
    c = zlog_get_category("paxos");
    zlog_info(c, "Paxos run UUID: %s | Detected change of state. Starting Paxos run.", detected_state->uuid);

    pthread_mutex_unlock(&detected_states_hashmap_lock);

    if(serialize_hash(nodes,
            &state->current_replica_state_buffer,
            &state->len, replica->current_node_ip,
            (char *) uuid) != 0 )
    {
        free(state->current_replica_state_buffer);
        free(state);
        return;
    }

    evpaxos_replica_submit(replica->paxos_replica, state->current_replica_state_buffer, (int)state->len);
    free(state->current_replica_state_buffer);
    free(state);
}

void terminate_paxos_replica() {
    event_base_loopexit(replica_local->base, NULL);
    pthread_cancel(thread_id);
    pthread_join(thread_id, NULL);
}

void *init_replica(void *arg)
{
    struct event_base* base;
    struct event* sig;
    struct args_struct* args = arg;
    replica_local = args->replica;
    int id = args->id;
    struct fd_replica* replica = args->replica;
    printf("Starting replica with id: %d \n", id);

    const char* config = "../paxos.conf";

    base = event_base_new();
    replica->base = base;
    replica->id = id;
    replica->paxos_replica = evpaxos_replica_init(id, config, on_deliver,
                                                 replica, base);

    if (replica->paxos_replica == NULL) {
        printf("Failed to initialize paxos replica\n");
        exit(1);
    } else {
        printf("Initializing paxos replica with id: %d\n", id);
    }

    init_state(replica);
    evpaxos_replica_set_instance_id(replica->paxos_replica, replica->instance_id);

    memset(&replica->trim, 0, sizeof(struct trim_info));
    int replica_count = evpaxos_replica_count(replica->paxos_replica);
    replica->trim.count = replica_count;

    pthread_mutex_lock( &paxos_initialization_mutex );
    done= true;
    printf( "[paxos replica thread] Done initializing. Signalling cond.\n");

    // Signal main thread that paxos replica is initialized
    pthread_cond_signal( &cond );
    pthread_mutex_unlock( & paxos_initialization_mutex );

    sig = evsignal_new(base, SIGINT, handle_sigint, base);
    evsignal_add(sig, NULL);

    event_base_dispatch(base);
    free(arg);
    event_free(sig);
    clean_up_replica(replica);
    event_base_free(base);
    pthread_exit(0);
}

void clean_up_replica(struct fd_replica *replica) {
    zlist_destroy (&replica->state->paxos_state_array);
    zlist_destroy (&replica->state->paxos_received_state_array);
    zlist_destroy (&replica->state->detected_state_array);
    free(replica->state);
    evpaxos_replica_free(replica->paxos_replica);
    clean_up_paxos_hashmap();
    pthread_mutex_destroy(&paxos_listener_mutex);
    pthread_mutex_destroy(&detected_states_hashmap_lock);
    free(replica);
}


int
start_paxos_replica(int id,  fd_replica* replica)
{
    signal(SIGPIPE, SIG_IGN);
    if (pthread_mutex_init(&paxos_initialization_mutex, NULL)){
        printf("ERROR: can't create mutex in paxos replica start \n");
        exit(-1);
    }

    if (pthread_cond_init(&cond, NULL)){
        printf("ERROR: can't create mutex in paxos replica start \n");
        exit(-1);
    }

    if (pthread_mutex_init(&paxos_listener_mutex, NULL)){
        printf("ERROR: can't create Paxos listener mutex \n");
        exit(-1);
    }

    if (pthread_mutex_init(&detected_states_hashmap_lock, NULL)){
        printf("ERROR: can't create Paxos hashmap mutex \n");
        exit(-1);
    }

    struct args_struct* arg = malloc(sizeof(struct args_struct));
    arg->id = id;
    arg->replica = replica;

    evthread_use_pthreads();

    if(pthread_create(&thread_id, NULL, (void *) &init_replica, (void *) arg)) {
        /*Thread creation failed*/
        return 0;
    }

    pthread_mutex_lock(&paxos_initialization_mutex);
    while(!done) {
        puts( "[thread main] done so waiting on cond\n");
        /* block this thread until another thread signals cond. While
       blocked, the mutex is released, then re-aquired before this
       thread is woken up and the call returns. */
        pthread_cond_wait( & cond, & paxos_initialization_mutex );
        puts( "[thread main] woken up - cond was signalled." );
    }

    printf( "[thread main] Paxos replica initialized - continue in main thread \n" );
    pthread_mutex_unlock( & paxos_initialization_mutex );

    // Clean up mutex
    pthread_mutex_destroy(&paxos_initialization_mutex);
    pthread_cond_destroy(&cond);
    return 1;
}


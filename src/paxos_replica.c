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
#include "utils.h"

struct timeval count_interval = {0, 0};
pthread_t thread_id;

struct trim_info
{
    int count;
    int last_trim;
    int instances[16];
};

struct fd_replica
{
    int id;
    char* state;
    unsigned instance_id;
    struct trim_info trim;
    struct event* client_ev;
    struct event* sig;
    struct evpaxos_replica* paxos_replica;
    struct event_base* base;
};

struct args_struct{
    int id;
    struct fd_replica* replica;
};

static void
handle_sigint(int sig, short ev, void* arg)
{
    struct event_base* base = arg;
    event_base_loopexit(base, NULL);
}

static void
init_state(struct fd_replica* replica)
{
    char filename[128];
//    replica->state =
    replica->instance_id = 0;
    snprintf(filename, sizeof(filename), "state-%d", replica->id);
    FILE* f = fopen(filename, "r");
    if (f != NULL) {
        fscanf(f, "%s %d", replica->state, &replica->instance_id);
        fclose(f);
    }
}

static void
checkpoint_state(struct fd_replica* replica)
{
	char filename[128];
	snprintf(filename, sizeof(filename), "state-%d", replica->id);
	FILE* f = fopen(filename, "w+");
	fprintf(f, "%s %d", replica->state, replica->instance_id);
	fclose(f);
}

static void
update_state(struct fd_replica* replica, unsigned iid)
{
//    Update membership state here
//	replica->state =
    replica->instance_id = iid;
}

static void
update_trim_info(struct fd_replica* replica, int replica_id, int trim_id)
{
    if (trim_id <= replica->trim.instances[replica_id])
        return;
    int i, min = replica->trim.instances[0];
    replica->trim.instances[replica_id] = trim_id;
    for (i = 0; i < replica->trim.count; i++)
        if (replica->trim.instances[i] < min)
            min = replica->trim.instances[i];
    if (min > replica->trim.last_trim) {
        replica->trim.last_trim = min;
        evpaxos_replica_send_trim(replica->paxos_replica, min);
    }
}

static void
submit_trim(struct fd_replica* replica)
{
    char trim[64];
    snprintf(trim, sizeof(trim), "TRIM %d %d", replica->id, replica->instance_id);
    evpaxos_replica_submit(replica->paxos_replica, trim, (int) (strlen(trim) + 1));
}

static void
on_deliver(unsigned iid, char* value, size_t size, void* arg)
{
    int replica_id, trim_id;
    int id;
    struct fd_replica* replica = (struct fd_replica*)arg;
    char* result = (char*)malloc(sizeof(char) * MAX_SIZE_IP_ADDRESS_STRING);
    deserialize_hash(value, size);
    printf("Value: %.64s, Size: %d \n", value, (int)size);
    
    if (sscanf(value, "TRIM %d %d", &replica_id, &trim_id) == 2) {
        update_trim_info(replica, replica_id, trim_id);
    } else {
        update_state(replica, iid);
    }


    if (iid % 100 == 0) {
        checkpoint_state(replica);
        submit_trim(replica);
    }
}

void paxos_submit_remove(struct fd_replica* replica, char* ip){
    char val[64];
    int id = ip_to_id(ip);
    snprintf(val, sizeof(val), "REMOVE %d", id);
    evpaxos_replica_submit(replica->paxos_replica, val, (int) (strlen(val) + 1));
}

void paxos_serialize_and_submit(
                struct fd_replica* replica,
                struct node_struct *nodes,
                pthread_rwlock_t hashmap_lock,
                struct membership_state *state){
    char* buffer;

    if(nodes == NULL){
        printf("paxos_serialize_and_submit : NULL nodes structure \n");
        return;
    }

    serialize_hash(nodes, hashmap_lock, &buffer, &state->len);
    printf("Printing hash \n");
    printf("---------------------- \n");
    print_hash(nodes, hashmap_lock);
    printf("---------------------- \n");
    printf("Buffer %s, Length: %d \n", buffer, state->len);
    evpaxos_replica_submit(replica->paxos_replica, buffer, state->len);
    free(buffer);
}



void paxos_submit_state(struct fd_replica* replica, char* buffer){
    evpaxos_replica_submit(replica->paxos_replica, buffer, (int) (strlen(buffer) + 1));
}

void terminate_paxos_replica() {
    pthread_cancel(thread_id);
    pthread_join(thread_id, NULL);
}

void *init_replica(void *arg)
{
    struct event_base* base;
    struct event* sig;
    struct args_struct* args = arg;
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

    sig = evsignal_new(base, SIGINT, handle_sigint, base);
    evsignal_add(sig, NULL);

    event_base_dispatch(base);
    free(arg);
    event_free(sig);
    event_free(replica->client_ev);
    evpaxos_replica_free(replica->paxos_replica);
    event_base_free(base);
    pthread_exit(0);
}


int
start_paxos_replica(int id, struct fd_replica* replica)
{
    signal(SIGPIPE, SIG_IGN);

    struct args_struct* arg = malloc(sizeof(struct args_struct));
    arg->id = id;
    arg->replica = replica;

    evthread_use_pthreads();

    if(pthread_create(&thread_id, NULL, (void *) &init_replica, (void *) arg)) {
        /*Thread creation failed*/
        return 0;
    }
    return 1;
}


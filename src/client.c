
#include <czmq.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <event.h>
#include <evpaxos.h>
#include "timer.h"
#include "adaptive_delay_model.h"
#include "uthash.h"
#include "paxos_replica.h"

#define PING_PORT_NUMBER 1500
#define PING_MSG_SIZE    1
#define PING_INTERVAL    1000  //  Once per second

typedef struct node_struct {
    char *ipaddress; /* key */
    double last_heartbeat_ms;
    size_t timer_id;
    adaptive_timeout_struct *timeout_metadata;
    UT_hash_handle hh;  /* makes this structure hashable */
} node_struct;

struct args_struct{
    int id;
    struct fd_replica* replica;
};

bool silenced = false;
int error;
double time_diff;
// Time variables
struct timeval current_time;
// Nodes hashmap
struct node_struct *nodes = NULL;
pthread_rwlock_t hashmap_lock;
// Failure detector PAXOS replica - acceptor, learner, proposer
struct fd_replica* replica;


double get_current_time_ms(){
    gettimeofday(&current_time, NULL);
    return (current_time.tv_sec + (current_time.tv_usec / 1e6))* 1e3;
}


void print_hash() {
    struct node_struct *n;
    if (pthread_rwlock_rdlock(&hashmap_lock) != 0) {
        printf("ERROR: can't get rdlock \n");
        return;
    }
    for(n=nodes; n != NULL; n=n->hh.next) {
        printf("IP addr %s: Last heartbeat: %f\n", n->ipaddress, n->last_heartbeat_ms);
    }
    pthread_rwlock_unlock(&hashmap_lock);
}



int ip_address_hash_sort_function(struct node_struct *a,struct node_struct *b) {
    return strcmp(a->ipaddress, b->ipaddress);
}

void handle_timeout(size_t timer_id, void * user_data) {
    struct node_struct *node = NULL;
    char *ip_address = (char *) user_data;

    //        Check if node in map
    if (pthread_rwlock_rdlock(&hashmap_lock) != 0) {
        printf("ERROR: can't get rdlock \n");
        return;
    }
    HASH_FIND_STR(nodes, ip_address, node);
    pthread_rwlock_unlock(&hashmap_lock);

    if(node != NULL){
        if (pthread_rwlock_wrlock(&hashmap_lock) != 0){
            printf("ERROR: can't get wrlock \n");
        }
        HASH_DEL(nodes, node);
        pthread_rwlock_unlock(&hashmap_lock);
    }

    char val[64];
    snprintf(val, sizeof(val), "REMOVE %d %s", 123, (char *)user_data);
    evpaxos_replica_submit(replica->paxos_replica, val, (int) (strlen(val) + 1));

    printf("Timer %d expired, ip: %s\n", (int)timer_id, (char *)user_data);
}


void clean_up_hashmap() {
    //    Clean up hashmap
    node_struct *current_node, *tmp;
    HASH_ITER(hh, nodes, current_node, tmp) {
        if (pthread_rwlock_wrlock(&hashmap_lock) != 0){
            printf("ERROR: can't get wrlock \n");
        }
        HASH_DEL(nodes, current_node);
        pthread_rwlock_unlock(&hashmap_lock);
        free(current_node->timeout_metadata->past_arrival_time_differences_w1_ms);
        free(current_node->timeout_metadata->past_arrival_time_differences_w2_ms);
        free(current_node->timeout_metadata->deltas_between_messages);
        free(current_node->timeout_metadata);
        free(current_node);
    }
}

int main(int argc, char **argv) {
    bool DEBUG = false;
    if(argc > 1 && streq(argv[1], "debug")){
        DEBUG = true;
    }

//    File to save adaptive delay results. Used for benchmarking
    FILE *fp;
    fp=fopen("file.csv","w+");


    //  Create new beacon
    zactor_t *speaker = zactor_new (zbeacon, NULL);
    zactor_t *listener = zactor_new (zbeacon, NULL);

    //  Enable verbose logging of commands and activity:
    zstr_sendx (speaker, "VERBOSE", NULL);
    zstr_sendx (listener, "VERBOSE", NULL);

    zsock_send (speaker, "si", "CONFIGURE", PING_PORT_NUMBER);
    zsock_send (listener, "si", "CONFIGURE", PING_PORT_NUMBER);
//    Get IP of current node
    char *hostname = zstr_recv(listener);

    //    Start PAXOS replica
    int id = atoi(&hostname[strlen(hostname) - 1]);
    struct args_struct* arg = malloc(sizeof(struct args_struct));
    arg->id = id;
    replica = malloc(sizeof(struct fd_replica));
    arg->replica = replica;
    start_paxos_replica(id, replica);
//    sleep(1);

//
//    struct event *e = event_new(replica->base, -1, EV_TIMEOUT, submit_value, arg);
//    event_add(e, &count_interval);
//    zstr_sendx (speaker, "PUBLISH", "STOP", "1000", NULL);
    zsock_send (speaker, "sbi", "PUBLISH", "!", PING_MSG_SIZE, PING_INTERVAL);
    silenced = false;
    // We will listen to anything (empty subscription)
    zsock_send (listener, "sb", "SUBSCRIBE", "", 0);

// Start timer management thread
    create_timer_manager();
//    Initialize hashmap lock
    if (pthread_rwlock_init(&hashmap_lock,NULL) != 0) {
        printf("ERROR: can't create rwlock \n");
        exit(-1);
    }

    while (!zctx_interrupted) {
//        zframe_t *content = zframe_recv (listener);
        node_struct *node = NULL;
        adaptive_timeout_struct *timeout_metadata = NULL;

        char *ipaddress, *received;
        zstr_recvx (listener, &ipaddress, &received, NULL);

        if(!streq(hostname, ipaddress) || DEBUG){
            if(DEBUG){
                printf("IP Address: %s, Data: %s \n", ipaddress, received);
            }

            double current_time_ms = get_current_time_ms();
           
            //        Check if node in hash
            if (pthread_rwlock_rdlock(&hashmap_lock) != 0) {
                printf("ERROR: can't get rdlock \n");
                break;
            }
            HASH_FIND_STR(nodes, ipaddress, node);
            pthread_rwlock_unlock(&hashmap_lock);

//            Node not in hash. Create a node struct and start a timeout timer.
            if(node == NULL){
                node = (struct node_struct *)malloc(sizeof *node);
                node->ipaddress = (char*) malloc(strlen(ipaddress));
                strcpy(node->ipaddress,ipaddress);
                node->last_heartbeat_ms = current_time_ms;
                node->timeout_metadata = init_adaptive_timeout_struct(current_time_ms);

                if (pthread_rwlock_wrlock(&hashmap_lock) != 0) {
                    printf("ERROR: can't get wrlock \n");
                    break;
                }
                HASH_ADD_KEYPTR(hh, nodes, node->ipaddress, strlen(node->ipaddress), node);
                pthread_rwlock_unlock(&hashmap_lock);
//                Adaptive estimation of the expected next delay
                estimate_next_delay(node->timeout_metadata, current_time_ms, DEBUG, fp);
                unsigned int timeout = (unsigned int) node->timeout_metadata->next_timeout;
                node->timer_id = start_timer(timeout, handle_timeout, TIMER_SINGLE_SHOT, node->ipaddress);
            } else {
//                Node in hash. Estimate the next delay based on information so far and restart the timer.
                stop_timer(node->timer_id);
                estimate_next_delay(node->timeout_metadata, current_time_ms, DEBUG, fp);
                unsigned int timeout = (unsigned int) node->timeout_metadata->next_timeout;
                node->timer_id = start_timer(timeout, handle_timeout, TIMER_SINGLE_SHOT, node->ipaddress);
                time_diff = (current_time_ms - node->last_heartbeat_ms);
                if(DEBUG){
                    printf("Time since last heartbeat  %2fms\n", time_diff);
                }
                node->last_heartbeat_ms= current_time_ms;
            }

//            Allow orchestrator to silence/restart the node.
            if(streq(hostname, received) && !silenced) {
                printf("Silencing.....\n");
                silenced = true;
                zstr_sendx (speaker, "SILENCE", NULL);
            } else if(streq(hostname, received) && silenced) {
                printf("Restarting....\n");
                zsock_send (speaker, "sbi", "PUBLISH", "!", PING_MSG_SIZE, PING_INTERVAL);
            }
            zstr_free (&received);
        }
        zstr_free (&ipaddress);
        zstr_free (&received);
    }

//    Stop timer management thread
    terminate_timer_manager();
//    Stop paxos replica thread
    terminate_paxos_replica();
//    Close opened file
    fclose(fp);
    // Wait for at most 1/2 second if there's no broadcasting
    zsock_set_rcvtimeo (listener, 5*PING_INTERVAL);;
    zactor_destroy (&listener);
    zactor_destroy (&speaker);

    clean_up_hashmap();
}
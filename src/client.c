
#include <czmq.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "timer.h"
#include "adaptive_delay_model.h"
#include "uthash.h"
#define PING_PORT_NUMBER 1500
#define PING_MSG_SIZE    1
#define PING_INTERVAL    1000  //  Once per second
#define TIMEOUT 2000000

typedef struct node_struct {
    char *ipaddress; /* key */
    double last_heartbeat_ms;
    size_t timer_id;
    adaptive_timeout_struct *timeout_metadata;
    UT_hash_handle hh;  /* makes this structure hashable */
} node_struct;

bool silenced = false;
int error;
double time_diff;
// Time variables
struct timeval  current_time;
// Nodes hashmap
struct node_struct *nodes = NULL;
pthread_rwlock_t hashmap_lock;

double get_current_time_ms(){
    gettimeofday(&current_time, NULL);
    return (current_time.tv_sec + (current_time.tv_usec / 1e6))* 1e3;
}


void print_hash() {
    struct node_struct *n;
    for(n=nodes; n != NULL; n=n->hh.next) {
        printf("IP addr %s: Last heartbeat: %f\n", n->ipaddress, n->last_heartbeat_ms);
    }
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
        free(current_node->timeout_metadata);
        free(current_node);
    }
}

int main(int argc, char **argv) {
    bool DEBUG = false;
    if(argc > 1 && streq(argv[1], "debug")){
        DEBUG = true;
    }

//    File to save adaptive delay results
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
    char *hostname = zstr_recv(listener);


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
            printf("IP Address: %s, Data: %s \n", ipaddress, received);

            double current_time_ms = get_current_time_ms();
           
            //        Check if node in map
            if (pthread_rwlock_rdlock(&hashmap_lock) != 0) {
                printf("ERROR: can't get rdlock \n");
                break;
            }
            HASH_FIND_STR(nodes, ipaddress, node);
            pthread_rwlock_unlock(&hashmap_lock);

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

                node->timer_id = start_timer(TIMEOUT, handle_timeout, TIMER_SINGLE_SHOT, node->ipaddress);
            } else {
                stop_timer(node->timer_id);
                node->timer_id = start_timer(TIMEOUT, handle_timeout, TIMER_SINGLE_SHOT, node->ipaddress);
                time_diff = (current_time_ms - node->last_heartbeat_ms);
                printf("Time since last heartbeat  %2fms\n", time_diff);
                node->last_heartbeat_ms= current_time_ms;
            }

            estimate_next_delay(node->timeout_metadata, current_time_ms, fp);

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
//    Close opened file
    fclose(fp);
    // Wait for at most 1/2 second if there's no broadcasting
    zsock_set_rcvtimeo (listener, 5*PING_INTERVAL);;
    zactor_destroy (&listener);
    zactor_destroy (&speaker);

    clean_up_hashmap();
}
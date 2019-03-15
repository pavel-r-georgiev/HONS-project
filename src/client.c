
#include <czmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <event.h>
#include <evpaxos.h>
#include <zlog.h>
#include "include/timer.h"
#include "include/adaptive_delay_model.h"
#include "uthash.h"
#include "include/paxos_replica.h"
#include "include/utils.h"

#define PING_PORT_NUMBER 1500
#define PING_MSG_SIZE    1
#define PING_INTERVAL    HEARTBEAT_INTERVAL //  In ms

bool DEBUG = false;

bool silenced = false;
int error;
double time_diff;

// Nodes hashmap
struct node_struct *nodes = NULL;

pthread_mutex_t hashmap_lock;
pthread_mutexattr_t attrmutex;
// Failure detector PAXOS replica - acceptor, learner, proposer
struct fd_replica* replica;


void handle_timeout(size_t timer_id, void * user_data) {
    struct node_struct *node = NULL;
    char *ip_address = (char *) user_data;
//    printf("%s timeout \n", ip_address);
    //        Check if node in map
    if (pthread_mutex_lock(&hashmap_lock) != 0) {
        printf("ERROR: can't get mutex \n");
        return;
    }
    HASH_FIND_STR(nodes, ip_address, node);

    if(node != NULL){
        if(node->timeout_metadata){
            free(node->timeout_metadata->past_arrival_time_differences_w1_ms);
            free(node->timeout_metadata->past_arrival_time_differences_w2_ms);
            free(node->timeout_metadata->deltas_between_messages);
            free(node->timeout_metadata);
        }
        HASH_DEL(nodes, node);
        free(node);
    }

    pthread_mutex_unlock(&hashmap_lock);
    dzlog_info("Timer expired. Ip: %s, Td: %f\n",  (char *)user_data, node->timeout_metadata->next_timeout);

    paxos_serialize_and_submit(replica, nodes, &hashmap_lock);

    if(DEBUG) {
        printf("Timer %d expired, ip: %s\n", (int)timer_id, (char *)user_data);
        print_hash(nodes, &hashmap_lock);
    }
}

// Add hostname in nodes struct.Useful when sending state to other nodes.
void init_nodes_struct(char* hostname){
    node_struct *node = NULL;
    node = (struct node_struct *)malloc(sizeof *node);
    node->ipaddress = (char*) malloc(strlen(hostname));
    strcpy(node->ipaddress,hostname);

    if (pthread_mutex_lock(&hashmap_lock) != 0) {
        printf("ERROR: can't get mutex \n");
        return;
    }
    HASH_ADD_KEYPTR(hh, nodes, node->ipaddress, strlen(node->ipaddress), node);
    pthread_mutex_unlock(&hashmap_lock);
}

/**
 * Initialize logger with a default category. Use dzlog_set_category(char *name) to change the category if needed.
 */
static void
init_logger() {
    //    Setup logging
    int rc;
    rc = dzlog_init("/etc/zlog.conf", "failure_detector");
    if (rc) {
        printf("Failed to setup logging library.\n");
    }

    dzlog_info("-------------------------------------");
    dzlog_info("START OF NEW LOG");
    dzlog_info("-------------------------------------");
}

void clean_up_hashmap() {
    //    Clean up hashmap
    node_struct *current_node, *tmp;
    HASH_ITER(hh, nodes, current_node, tmp) {
        if (pthread_mutex_unlock(&hashmap_lock) != 0){
            printf("ERROR: can't get mutex \n");
        }
        if(current_node->timeout_metadata){
            free(current_node->timeout_metadata->past_arrival_time_differences_w1_ms);
            free(current_node->timeout_metadata->past_arrival_time_differences_w2_ms);
            free(current_node->timeout_metadata->deltas_between_messages);
            free(current_node->timeout_metadata);
        }
        HASH_DEL(nodes, current_node);
        pthread_mutex_unlock(&hashmap_lock);
        free(current_node);
    }
}

int main(int argc, char **argv) {
    if(argc > 1 && streq(argv[1], "debug")){
        DEBUG = true;
    }


//    Initialize logger
    init_logger();

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

    init_nodes_struct(hostname);

    //    Start PAXOS replica
    int id = ip_to_id(hostname);

    replica = malloc(sizeof(struct fd_replica));
    start_paxos_replica(id, replica);

    zsock_send (speaker, "sbi", "PUBLISH", "!", PING_MSG_SIZE, PING_INTERVAL);
    silenced = false;
    // We will listen to anything (empty subscription)
    zsock_send (listener, "sb", "SUBSCRIBE", "", 0);

// Start timer management thread
    create_timer_manager();

    // Initialise attribute to mutex.
    pthread_mutexattr_init(&attrmutex);
    pthread_mutexattr_setpshared(&attrmutex, PTHREAD_PROCESS_SHARED);

    // Initialise hash mutex
    if (pthread_mutex_init(&hashmap_lock, &attrmutex)){
        printf("ERROR: can't create mutex \n");
        exit(-1);
    }

    while (!zctx_interrupted) {
        node_struct *node = NULL;

        char *ipaddress, *received;
        zstr_recvx (listener, &ipaddress, &received, NULL);

        if(!streq(hostname, ipaddress)){
            if(DEBUG){
                printf("IP Address: %s, Data: %s \n", ipaddress, received);
            }

            double current_time_ms = get_current_time_ms();
           
            //        Check if node in hash
            if (pthread_mutex_lock(&hashmap_lock) != 0) {
                printf("ERROR: can't get mutex \n");
                break;
            }
            HASH_FIND_STR(nodes, ipaddress, node);
            pthread_mutex_unlock(&hashmap_lock);

//            Node not in hash. Create a node struct and start a timeout timer.
            if(node == NULL){
                node = (struct node_struct *)malloc(sizeof *node);
                node->ipaddress = (char*) malloc(strlen(ipaddress));
                strcpy(node->ipaddress,ipaddress);
                node->last_heartbeat_ms = current_time_ms;
                node->timeout_metadata = init_adaptive_timeout_struct(current_time_ms);

                HASH_ADD_KEYPTR(hh, nodes, node->ipaddress, strlen(node->ipaddress), node);
                //                Adaptive estimation of the expected next delay
                estimate_next_delay(node->timeout_metadata, current_time_ms, DEBUG);
                unsigned int timeout = (unsigned int) node->timeout_metadata->next_timeout;
                node->timer_id = start_timer(timeout, handle_timeout, TIMER_SINGLE_SHOT, node->ipaddress);
                paxos_serialize_and_submit(replica, nodes, &hashmap_lock);
            } else {
//                Node in hash. Estimate the next delay based on information so far and restart the timer.
                stop_timer(node->timer_id);
                estimate_next_delay(node->timeout_metadata, current_time_ms, DEBUG);
                unsigned int timeout = (unsigned int) node->timeout_metadata->next_timeout;
                node->timer_id = start_timer(timeout, handle_timeout, TIMER_SINGLE_SHOT, node->ipaddress);
                time_diff = (current_time_ms - node->last_heartbeat_ms);
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
// Destroy logger
    zlog_fini();

    // Clean up mutex
    pthread_mutex_destroy(&hashmap_lock);
    pthread_mutexattr_destroy(&attrmutex);

//    Destroy CZMQ z_actors
    zactor_destroy (&listener);
    zactor_destroy (&speaker);

    clean_up_hashmap();
}
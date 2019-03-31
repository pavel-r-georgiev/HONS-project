
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
struct node_struct *nodes = NULL; /* important! initialize to NULL */

pthread_mutex_t hashmap_lock;
pthread_mutexattr_t attrmutex;
// Failure detector PAXOS replica - acceptor, learner, proposer
struct fd_replica* replica;
struct timeout_args_struct *args;


void handle_timeout(size_t timer_id, struct timeout_args_struct args) {
    struct node_struct *node = NULL;
//    printf("%s timeout \n", ip_address);
    //        Check if node in map
    if (pthread_mutex_lock(args.hashmap_lock) != 0) {
        printf("ERROR: can't get mutex \n");
        return;
    }
    HASH_FIND_STR(*args.nodes_p, args.ip_address, node);
    pthread_mutex_unlock(args.hashmap_lock);

    if(node != NULL){
        dzlog_info("Timer for IP %s expired, Detection time: %f",  args.ip_address, node->timeout_metadata->next_timeout);

        if(DEBUG) {
            printf("Timer expired. Ip: %s, Td: %f\n",  args.ip_address, node->timeout_metadata->next_timeout);
            print_hash(args.nodes_p, &hashmap_lock);
            printf("t: %f, k: %lld, Avg hb: %f EA: %f, FP: %f\n", node->timeout_metadata->arrival_time_ms,
                   node->timeout_metadata->seq_num,
                   node->timeout_metadata->average_heartbeat_time_ms,
                   node->timeout_metadata->estimated_arrival_ms,
                   node->timeout_metadata->freshness_point);
        }

        if (pthread_mutex_lock(args.hashmap_lock) != 0) {
            printf("ERROR: can't get mutex \n");
            return;
        }
        HASH_DEL(*args.nodes_p, node);
        pthread_mutex_unlock(args.hashmap_lock);

        if(node->timeout_metadata){
            free(node->timeout_metadata->past_arrival_time_differences_w1_ms);
            free(node->timeout_metadata->past_arrival_time_differences_w2_ms);
            free(node->timeout_metadata->deltas_between_messages);
            free(node->timeout_metadata);
        }
        free(node);

        paxos_serialize_and_submit(args.replica, args.nodes_p, args.hashmap_lock);
    }
}

// Add hostname in nodes struct.Useful when sending state to other nodes.
void init_nodes_struct(char* hostname){
//    Add current node to struct
    node_struct *node = NULL;
    node = malloc(sizeof(struct node_struct));
    strcpy(node->ipaddress,hostname);

    if (pthread_mutex_lock(&hashmap_lock) != 0) {
        printf("ERROR: can't get mutex \n");
        return;
    }
    HASH_ADD_STR(nodes, ipaddress, node);
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
        HASH_DEL(nodes, current_node);
        pthread_mutex_unlock(&hashmap_lock);
        if(current_node->timeout_metadata){
            free(current_node->timeout_metadata->past_arrival_time_differences_w1_ms);
            free(current_node->timeout_metadata->past_arrival_time_differences_w2_ms);
            free(current_node->timeout_metadata->deltas_between_messages);
            free(current_node->timeout_metadata);
        }
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
    strcpy(replica->current_node_ip, hostname);
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

//    Initialize args struct to pass to timeout function
    args = malloc(sizeof(struct timeout_args_struct));
    args->hashmap_lock = &hashmap_lock;
    args->nodes_p = &nodes;
    args->replica = replica;

    while (!zctx_interrupted) {
        node_struct *node = NULL;

        char *ipaddress, *received;

        if(zstr_recvx (listener, &ipaddress, &received, NULL) == -1){
            printf("ERROR: message could not be read \n");
            continue;
        }

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
                node = malloc(sizeof(struct node_struct));
                strcpy(node->ipaddress,ipaddress);
                node->last_heartbeat_ms = current_time_ms;
                node->timeout_metadata = init_adaptive_timeout_struct(current_time_ms, ipaddress);

                if (pthread_mutex_lock(&hashmap_lock) != 0) {
                    printf("ERROR: can't get mutex \n");
                    break;
                }
                HASH_ADD_STR(nodes, ipaddress, node);
//                Adaptive estimation of the expected next delay
                estimate_next_delay(node->timeout_metadata, current_time_ms, DEBUG);
                unsigned int timeout = (unsigned int) node->timeout_metadata->next_timeout;
                pthread_mutex_unlock(&hashmap_lock);
                strcpy(args->ip_address,ipaddress);
                node->timer_id = start_timer(timeout, handle_timeout, TIMER_SINGLE_SHOT, *args);
                paxos_serialize_and_submit(replica, &nodes, &hashmap_lock);
            } else {
//                Node in hash. Estimate the next delay based on information so far and restart the timer.
                if (pthread_mutex_lock(&hashmap_lock) != 0) {
                    printf("ERROR: can't get mutex \n");
                    break;
                }
                stop_timer(node->timer_id);
                estimate_next_delay(node->timeout_metadata, current_time_ms, DEBUG);
                unsigned int timeout = (unsigned int) node->timeout_metadata->next_timeout;
                pthread_mutex_unlock(&hashmap_lock);
                strcpy(args->ip_address,ipaddress);
                node->timer_id = start_timer(timeout, handle_timeout, TIMER_SINGLE_SHOT, *args);
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

#include <czmq.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "timer.h"
#include "uthash.h"
#define PING_PORT_NUMBER 1500
#define PING_MSG_SIZE    1
#define PING_INTERVAL    1000  //  Once per second
#define TIMEOUT 1200

typedef struct node_struct {
    char *ipaddress; /* key */
    double last_heartbeat_ms;
    UT_hash_handle hh;  /* makes this structure hashable */
} node_struct;

bool silenced = false;
int error;
double time_diff;
// Time variables
struct timeval  current_time;
// Nodes hashmap
struct node_struct *nodes = NULL;

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

int main(void) {
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

    while (!zctx_interrupted) {
//        zframe_t *content = zframe_recv (listener);
        struct node_struct *node = NULL;
        char *ipaddress, *received;
        zstr_recvx (listener, &ipaddress, &received, NULL);

        if(!streq(hostname, ipaddress)){
            printf("IP Address: %s, Data: %s \n", ipaddress, received);

            double current_time_ms = get_current_time_ms();
            //        Check if node in map
            HASH_FIND_STR(nodes, ipaddress, node);

            if(node == NULL){
                node = (struct node_struct *)malloc(sizeof *node);
                node->ipaddress = (char*) malloc(strlen(ipaddress));
                strcpy(node->ipaddress,ipaddress);
                node->last_heartbeat_ms = current_time_ms;
                HASH_ADD_KEYPTR(hh, nodes, node->ipaddress, strlen(node->ipaddress), node);
            } else {
                print_hash();
                time_diff = (current_time_ms - node->last_heartbeat_ms);
                printf("Time since last heartbeat  %2fms\n", time_diff);
                node->last_heartbeat_ms= current_time_ms;
            }

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

    // Wait for at most 1/2 second if there's no broadcasting
    zsock_set_rcvtimeo (listener, 5*PING_INTERVAL);;
    zactor_destroy (&listener);
    zactor_destroy (&speaker);

//    Clean up hashmap
    struct node_struct *current_node, *tmp;
    HASH_ITER(hh, nodes, current_node, tmp) {
        HASH_DEL(nodes, current_node);
        free(current_node);
    }
}

#ifndef HONS_PROJECT_UTILS_H
#define HONS_PROJECT_UTILS_H

#include <czmq.h>
#include "adaptive_delay_model.h"
#include "uthash.h"

#define MAX_SIZE_IP_ADDRESS_STRING 16 // 255.255.255.255
#define MAX_NUM_NODES 3


typedef struct node_struct {
    char ipaddress[MAX_SIZE_IP_ADDRESS_STRING]; /* key */
    double last_heartbeat_ms;
    size_t timer_id;
    adaptive_timeout_struct *timeout_metadata;
    UT_hash_handle hh;  /* makes this structure hashable */
} node_struct;


typedef struct timeout_args_struct{
    char ip_address[MAX_SIZE_IP_ADDRESS_STRING];
    node_struct **nodes_p;
    pthread_mutex_t *hashmap_lock;
    struct fd_replica* replica;
} timeout_args_struct;

typedef struct membership_state {
    zlist_t* paxos_state_array;
    char *current_replica_state_buffer;
    size_t len;
} membership_state;

//Important to pass pointer of pointer to nodes. UTHash macros change the pointer too.
void print_hash(struct node_struct **nodes, pthread_mutex_t *hashmap_lock);

int ip_to_id(char *ip);
//Important to pass pointer of pointer to nodes. UTHash macros change the pointer too.
void serialize_hash(struct node_struct **nodes, pthread_mutex_t *hashmap_lock, char** buffer, size_t* size);
void deserialize_hash(char* buffer,  size_t len, zlist_t* result);
void print_string_list(zlist_t* list);
double get_current_time_ms();
void log_state_list(zlist_t* list, double time_passed);

#endif //HONS_PROJECT_UTILS_H

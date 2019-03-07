
#ifndef HONS_PROJECT_UTILS_H
#define HONS_PROJECT_UTILS_H

#include <czmq.h>
#include "adaptive_delay_model.h"
#include "uthash.h"

#define MAX_SIZE_IP_ADDRESS_STRING 16
#define MAX_NUM_NODES 3

typedef struct node_struct {
    char *ipaddress; /* key */
    double last_heartbeat_ms;
    size_t timer_id;
    adaptive_timeout_struct *timeout_metadata;
    UT_hash_handle hh;  /* makes this structure hashable */
} node_struct;

typedef struct membership_state {
    zlist_t* paxos_state_array;
    size_t paxos_array_len;
    char *current_replica_state_buffer;
    size_t len;
} membership_state;

void print_hash(struct node_struct *nodes, pthread_rwlock_t hashmap_lock);

int ip_to_id(char *ip);

void serialize_hash(struct node_struct *nodes, pthread_rwlock_t hashmap_lock, char** buffer, size_t* size);
void deserialize_hash(char* buffer,  size_t len, zlist_t* result, size_t* array_length);
void print_string_list(zlist_t* list);
#endif //HONS_PROJECT_UTILS_H

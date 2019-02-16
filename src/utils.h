
#ifndef HONS_PROJECT_UTILS_H
#define HONS_PROJECT_UTILS_H

#include "adaptive_delay_model.h"
#include "uthash.h"

#define MAX_SIZE_IP_ADDRESS_STRING 16
#define MAX_NUM_NODES 5

typedef struct node_struct {
    char *ipaddress; /* key */
    double last_heartbeat_ms;
    size_t timer_id;
    adaptive_timeout_struct *timeout_metadata;
    UT_hash_handle hh;  /* makes this structure hashable */
} node_struct;

typedef struct membership_state {
    char *state_buffer;
    int len;
} membership_state;

void print_hash(struct node_struct *nodes, pthread_rwlock_t hashmap_lock);

int ip_to_id(char *ip);

void serialize_hash(struct node_struct *nodes, pthread_rwlock_t hashmap_lock, char** buffer, int* size);
void deserialize_hash(char* buffer,  size_t len);

#endif //HONS_PROJECT_UTILS_H

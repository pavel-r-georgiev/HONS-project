
#ifndef HONS_PROJECT_UTILS_H
#define HONS_PROJECT_UTILS_H

#include "adaptive_delay_model.h"
#include "uthash.h"

typedef struct node_struct {
    char *ipaddress; /* key */
    double last_heartbeat_ms;
    size_t timer_id;
    adaptive_timeout_struct *timeout_metadata;
    UT_hash_handle hh;  /* makes this structure hashable */
} node_struct;

void print_hash(struct node_struct *nodes, pthread_rwlock_t hashmap_lock);

int ip_to_id(char *ip);

#endif //HONS_PROJECT_UTILS_H

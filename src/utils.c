
#include <pthread.h>
#include <stdio.h>
#include <evpaxos.h>
#include "utils.h"


void print_hash(struct node_struct *nodes, pthread_rwlock_t hashmap_lock) {
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

void serialize_hash(struct node_struct *nodes, char* message, pthread_rwlock_t hashmap_lock){
    if(nodes == NULL){
        printf("ERROR: NULL nodes structure \n");
        return;
    }
    int size = sizeof(nodes->ipaddress);

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

int ip_to_id(char *ip){
    return atoi(&ip[strlen(ip) - 1]);
}
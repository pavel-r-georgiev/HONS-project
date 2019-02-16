
#include <pthread.h>
#include <stdio.h>
#include <evpaxos.h>
#include "utils.h"
#include "tpl.h"
#include "paxos_replica.h"


void print_hash(struct node_struct *nodes, pthread_rwlock_t hashmap_lock) {
    if(nodes == NULL){
        printf("ERROR: NULL nodes structure \n");
        return;
    }

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

void serialize_hash(struct node_struct *nodes, pthread_rwlock_t hashmap_lock, char** buffer, int* size){
    if(nodes == NULL){
        printf("ERROR: NULL nodes structure \n");
        return;
    }


    struct node_struct *n;
    if (pthread_rwlock_rdlock(&hashmap_lock) != 0) {
        printf("ERROR: can't get rdlock \n");
        return;
    }
//    Sort hash so that every state across the nodes is in same order
    HASH_SORT(nodes, ip_address_hash_sort_function);
    char* buf;
    tpl_node* tn;
    size_t len;
    char* ipaddress = (char*)malloc(sizeof(char) * MAX_SIZE_IP_ADDRESS_STRING);
    tn = tpl_map("A(s)", &ipaddress);

    for(n=nodes; n != NULL; n=n->hh.next) {
        strcpy(ipaddress, n->ipaddress);
        tpl_pack(tn, 1);
    }

    tpl_dump( tn, TPL_MEM, buffer, &len);
    printf("Serializing buffer %s size: %d \n", *buffer, (int)len);
    *size = (int) len;
    tpl_free(tn);
    free(ipaddress);
    pthread_rwlock_unlock(&hashmap_lock);
}

void deserialize_hash(char* buffer,  size_t len){
    char *result;
    tpl_node* tn;

    tn = tpl_map( "A(s)", &result );
    tpl_load( tn, TPL_MEM, buffer, len);
    printf("DESERIALIZED \n");
    while (tpl_unpack(tn,1) > 0) printf("%s\n", result);

    tpl_free( tn );
//    free( buffer );
}

int ip_to_id(char *ip){
    return atoi(&ip[strlen(ip) - 1]);
}
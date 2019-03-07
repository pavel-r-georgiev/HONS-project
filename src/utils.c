
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
    printf("---------------------- \n");
    for(n=nodes; n != NULL; n=n->hh.next) {
        printf("IP addr %s: Last heartbeat: %f\n", n->ipaddress, n->last_heartbeat_ms);
    }
    printf("---------------------- \n");
    pthread_rwlock_unlock(&hashmap_lock);
}

int ip_address_hash_sort_function(struct node_struct *a,struct node_struct *b) {
    return strcmp(a->ipaddress, b->ipaddress);
}

void serialize_hash(struct node_struct *nodes, pthread_rwlock_t hashmap_lock, char** buffer, size_t* size){
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
    *size = len;
    tpl_free(tn);
    free(ipaddress);
    pthread_rwlock_unlock(&hashmap_lock);
}

void deserialize_hash(char* buffer,  size_t len, zlist_t* result, size_t* array_length){
    char *temp;
    tpl_node* tn;
    printf("Deserializing buffer: %s  len: %d\n", buffer, (int)len);
    tn = tpl_map( "A(s)", &temp );
    tpl_load( tn, TPL_MEM, buffer, len);

    while (tpl_unpack(tn,1) > 0){
        zlist_push (result,  temp);
    }

    printf("Received new state from another node: \n");
    print_string_list(result);
    tpl_free( tn );
    free(temp);
}

int ip_to_id(char *ip){
    return atoi(&ip[strlen(ip) - 1]);
}

void print_string_list(zlist_t* list){
    if(list == NULL){
        printf("ERROR: NULL list pointer  \n");
        return;
    }

    char* str = zlist_first(list);
    printf("---------------------- \n");
    while(str != NULL){
        printf("%s \n", str);
        str = zlist_next(list);
    }
    printf("---------------------- \n");
    free(str);
}
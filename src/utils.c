
#include <pthread.h>
#include <stdio.h>
#include <evpaxos.h>
#include <zlog.h>
#include "include/utils.h"
#include "include/tpl.h"
#include "include/paxos_replica.h"


const char* TIME_PASSED_LABEL = "Since last state change: ";
void print_hash(struct node_struct *nodes, pthread_mutex_t *hashmap_lock) {
    if(nodes == NULL){
        printf("ERROR: NULL nodes structure \n");
        return;
    }

    struct node_struct *n;
    if (pthread_mutex_lock(hashmap_lock) != 0) {
        printf("ERROR: can't get mutex \n");
        return;
    }
    printf("---------------------- \n");
    for(n=nodes; n != NULL; n=n->hh.next) {
        printf("IP addr %s: Last heartbeat: %f\n", n->ipaddress, n->last_heartbeat_ms);
    }
    printf("---------------------- \n");
    pthread_mutex_unlock(hashmap_lock);
}

int ip_address_hash_sort_function(struct node_struct *a,struct node_struct *b) {
    return strcmp(a->ipaddress, b->ipaddress);
}

void serialize_hash(struct node_struct *nodes, pthread_mutex_t *hashmap_lock, char** buffer, size_t* size){
    if(nodes == NULL){
        printf("ERROR: NULL nodes structure \n");
        return;
    }


    struct node_struct *n;
    if (pthread_mutex_lock(hashmap_lock) != 0) {
        printf("ERROR: can't get mutex \n");
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
//    printf("Serializing buffer %s size: %d \n", *buffer, (int)len);
    *size = len;
    tpl_free(tn);
    free(ipaddress);
    pthread_mutex_unlock(hashmap_lock);
}

void deserialize_hash(char* buffer,  size_t len, zlist_t* result, size_t* array_length){
    char *temp = malloc(sizeof(char) * MAX_SIZE_IP_ADDRESS_STRING);

    tpl_node* tn;
//    printf("Deserializing buffer: %s  len: %d\n", buffer, (int)len);
    tn = tpl_map( "A(s)", &temp );
    tpl_load( tn, TPL_MEM, buffer, len);

    while (tpl_unpack(tn,1) > 0){
        zlist_push (result,  temp);
    }

//    printf("Received new state from another node: \n");
//    print_string_list(result);
    tpl_free( tn );

    free(temp);
}

double get_current_time_ms(){
    return  zclock_usecs()/1e3;
}

int ip_to_id(char *ip){
    return atoi(&ip[strlen(ip) - 1]);
}

void log_state_list(zlist_t* list, double time_passed) {
    size_t buffer_size = (zlist_size(list) * (MAX_SIZE_IP_ADDRESS_STRING + 4)) +
            sizeof(double) + 2 +
            sizeof(char) * strlen(TIME_PASSED_LABEL) + 4;

//    Use variable so I don't have to deal with dynamic memory alloc. Sorry not sorry.
    char a[buffer_size];
    char* buffer = a;
    char* str = zlist_first(list);

    while(str != NULL){
        buffer += sprintf(buffer, "| %s", str);
        str = zlist_next(list);
    }

    sprintf(buffer, "| %s %f ms", TIME_PASSED_LABEL, time_passed);
    dzlog_info(a);
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
}
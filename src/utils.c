
#include <pthread.h>
#include <stdio.h>
#include <evpaxos.h>
#include <zlog.h>
#include "include/utils.h"
#include "include/tpl.h"
#include "include/paxos_replica.h"


const char* TIME_PASSED_LABEL = "Since last state change: ";
void print_hash(struct node_struct **nodes, pthread_mutex_t *hashmap_lock) {
    if(nodes == NULL || *nodes == NULL){
        printf("ERROR: NULL nodes structure \n");
        return;
    }

    struct node_struct *n;
    if (pthread_mutex_lock(hashmap_lock) != 0) {
        printf("ERROR: can't get mutex \n");
        return;
    }
    printf("---------------------- \n");
    for(n=*nodes; n != NULL; n=n->hh.next) {
        printf("IP addr %s: Last heartbeat: %f\n", n->ipaddress, n->last_heartbeat_ms);
    }
    printf("---------------------- \n");
    pthread_mutex_unlock(hashmap_lock);
}

int ip_address_hash_sort_function(struct node_struct *a,struct node_struct *b) {
    return strcmp(a->ipaddress, b->ipaddress);
}

int serialize_hash(struct node_struct **nodes,
        pthread_mutex_t *hashmap_lock,
        char** buffer,
        size_t* size,
        char current_node_ip[]){
    if(nodes == NULL || *nodes == NULL){
        printf("ERROR: NULL nodes structure \n");
        return -1;
    }

    struct node_struct *n;
    if (pthread_mutex_lock(hashmap_lock) != 0) {
        printf("ERROR: can't get mutex \n");
        return -1;
    }
//    Sort hash so that every state across the nodes is in same order
    HASH_SORT(*nodes, ip_address_hash_sort_function);
    tpl_node* tn;
    size_t len;
    char ipaddress[MAX_SIZE_IP_ADDRESS_STRING];
//    Serialized message includes: IP address of current node, state array
    tn = tpl_map("c#A(c#)", current_node_ip, MAX_SIZE_IP_ADDRESS_STRING, &ipaddress, MAX_SIZE_IP_ADDRESS_STRING);

//   Pack current ip address
    tpl_pack(tn, 0);

    for(n=*nodes; n != NULL; n=n->hh.next) {
        strcpy(ipaddress, n->ipaddress);
        tpl_pack(tn, 1);
    }

    if(tpl_dump( tn, TPL_MEM, buffer, &len) == -1){
        puts("ERROR: error when dumping to buffer in serialize hash function");
        tpl_free(tn);
        pthread_mutex_unlock(hashmap_lock);
        return -1;
    }
//    printf("Serializing buffer %s size: %d \n", *buffer, (int)len);
    *size = len;
    tpl_free(tn);
    pthread_mutex_unlock(hashmap_lock);

    return 0;
}


void get_membership_group_from_hash(struct node_struct **nodes, pthread_mutex_t *hashmap_lock, zlist_t* result) {
    if (pthread_mutex_lock(hashmap_lock) != 0) {
        printf("ERROR: can't get mutex \n");
        return;
    }
    char temp[MAX_SIZE_IP_ADDRESS_STRING];
    struct node_struct *n;
//    Sort hash so that every state across the nodes is in same order
    HASH_SORT(*nodes, ip_address_hash_sort_function);
    for(n=*nodes; n != NULL; n=n->hh.next) {
        strcpy(temp, n->ipaddress);
        zlist_push(result, temp);
    }
    pthread_mutex_unlock(hashmap_lock);
}

bool is_equal_lists(zlist_t *l1, zlist_t *l2) {
    if(zlist_size(l1) == 0 || zlist_size(l2) == 0 || l1 == NULL || l2 == NULL){
        return false;
    }

//    if(l1 == NULL || l2 == NULL){
//        return l1 == l2;
//    }
//
//    if(zlist_size(l1) == 0 || zlist_size(l2) == 0){
//        return zlist_size(l1) == zlist_size(l2);
//    }

    if(zlist_size(l1) != zlist_size(l2)){
        return false;
    }

    char* str_l1 = zlist_first(l1);
    char* str_l2 = zlist_first(l2);

    while(str_l1 != NULL && str_l2 != NULL){
        if(strcmp(str_l1, str_l2) != 0){
            return false;
        }
        str_l1 = zlist_next(l1);
        str_l2 = zlist_next(l2);
    }

    return str_l1 == NULL && str_l2 == NULL;
}

int deserialize_hash(char* buffer,  size_t len, zlist_t* result, char* sender_ip){
    char temp[MAX_SIZE_IP_ADDRESS_STRING];

    tpl_node* tn;
//    printf("Deserializing buffer: %s  len: %d\n", buffer, (int)len);
    tn = tpl_map( "c#A(c#)", sender_ip, MAX_SIZE_IP_ADDRESS_STRING, &temp, MAX_SIZE_IP_ADDRESS_STRING);
    if(tpl_load( tn, TPL_MEM, buffer, len) == -1){
        puts("ERROR: Loading buffer in deserialize function unsuccessful");
        tpl_free( tn );
        return -1;
    }
//    Unpack sender ip
    tpl_unpack(tn,0);
//    Unpack state array
    while (tpl_unpack(tn,1) > 0){
        zlist_push (result,  temp);
    }

//    printf("Received new state from another node: \n");
//    print_string_list(result);
    tpl_free( tn );
    return 0;
}

void copy_list(zlist_t *source, zlist_t *dest) {
    zlist_purge(dest);

    char* str = zlist_first(source);

    while(str != NULL){
        zlist_append(dest, str);
        str = zlist_next(source);
    }
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
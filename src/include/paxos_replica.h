
#ifndef HONS_PROJECT_PAXOS_REPLICA_H
#define HONS_PROJECT_PAXOS_REPLICA_H

#include "utils.h"
struct trim_info
{
    int count;
    int last_trim;
    int instances[16];
};

typedef struct fd_replica
{
    int id;
    char current_node_ip[MAX_SIZE_IP_ADDRESS_STRING];
    struct membership_state* state;
    unsigned instance_id;
    struct trim_info trim;
    struct event* client_ev;
    struct event* sig;
    struct evpaxos_replica* paxos_replica;
    struct event_base* base;
} fd_replica;

typedef struct detected_state_struct {
    char* uuid; /* key */
    double time_discovered;
    UT_hash_handle hh;  /* makes this structure hashable */
} detected_state_struct;

extern pthread_mutex_t paxos_listener_mutex;
extern pthread_mutex_t detected_states_hashmap_lock;

int start_paxos_replica(int id,  fd_replica* replica);
void clean_up_replica(struct fd_replica *replica);
void terminate_paxos_replica();
void paxos_serialize_and_submit(
        struct fd_replica* replica,
        struct node_struct **nodes //Important to pass pointer of pointer to nodes. UTHash macros change the pointer too.
        );


#endif //HONS_PROJECT_PAXOS_REPLICA_H


#ifndef HONS_PROJECT_PAXOS_REPLICA_H
#define HONS_PROJECT_PAXOS_REPLICA_H

#include "utils.h"
struct trim_info
{
    int count;
    int last_trim;
    int instances[16];
};

struct fd_replica
{
    int id;
    char* state;
    unsigned instance_id;
    struct trim_info trim;
    struct event* client_ev;
    struct event* sig;
    struct evpaxos_replica* paxos_replica;
    struct event_base* base;
};


void start_paxos_replica(int id, struct fd_replica* replica);
void terminate_paxos_replica();
void paxos_serialize_and_submit(
        struct fd_replica* replica,
        struct node_struct *nodes,
        pthread_rwlock_t hashmap_lock,
        struct membership_state *state);


#endif //HONS_PROJECT_PAXOS_REPLICA_H

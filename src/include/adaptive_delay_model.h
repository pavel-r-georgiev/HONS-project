
#ifndef HONS_PROJECT_ADAPTIVE_DELAY_MODEL_H
#define HONS_PROJECT_ADAPTIVE_DELAY_MODEL_H
#include<stdio.h>
#include <stdbool.h>

#define W1_WINDOW_SIZE 10
#define W2_WINDOW_SIZE 10000
#define HEARTBEAT_INTERVAL 50
#define INITIAL_DELAY 5000
#define MAX_SIZE_IP_ADDRESS_STRING 16 // 255.255.255.255

typedef struct adaptive_timeout_struct {
    double freshness_point;
    double arrival_time_ms;
    double first_heartbeat_arrival_ms;
    double* past_arrival_time_differences_w1_ms;
    double* past_arrival_time_differences_w2_ms;
    double* deltas_between_messages;
    double estimated_arrival_ms;
    double delay;
    double alpha;
    double var;
    double error;
    long long seq_num;
    double sum_w1;
    double sum_w2;
    double sum_time_deltas;
    double next_timeout;
    double average_heartbeat_time_ms;
    char node_ip[MAX_SIZE_IP_ADDRESS_STRING];
    bool notified_windows_full;
} adaptive_timeout_struct;

adaptive_timeout_struct* init_adaptive_timeout_struct(double first_heartbeat_ms, char* node_ip);
void estimate_next_delay(adaptive_timeout_struct *heartbeat_metadata,  double current_arrival_time, bool DEBUG);

#endif //HONS_PROJECT_ADAPTIVE_DELAY_MODEL_H


#ifndef HONS_PROJECT_ADAPTIVE_DELAY_MODEL_H
#define HONS_PROJECT_ADAPTIVE_DELAY_MODEL_H
#include<stdio.h>

#define INITIAL_DELAY 5000
#define W1_WINDOW_SIZE 5
#define W2_WINDOW_SIZE 1000
#define HEARTBEAT_INTERVAL 1000

typedef struct adaptive_timeout_struct {
    double freshness_delta;
    double freshness_point;
    double arrival_time_ms;
    double first_heartbeat_arrival_ms;
    double* past_arrival_time_differences_w1_ms;
    double* past_arrival_time_differences_w2_ms;
    double* time_between_messages;
    double estimated_arrival_ms;
    double delay;
    double alpha;
    double var;
    double error;
    long seq_num;
    double sum_w1;
    double sum_w2;
} adaptive_timeout_struct;

adaptive_timeout_struct* init_adaptive_timeout_struct(double first_heartbeat_ms);
void estimate_next_delay(adaptive_timeout_struct *heartbeat_metadata,  double current_arrival_time, FILE *fp);

#endif //HONS_PROJECT_ADAPTIVE_DELAY_MODEL_H

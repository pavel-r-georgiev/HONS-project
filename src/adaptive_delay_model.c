
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <zlog.h>
#include <string.h>
#include "include/adaptive_delay_model.h"
// BETA and GAMMA determine how much weight to give past samples
#define  BETA 0.125
#define GAMMA 0.9
// Determines how much deviation from the mean to tolerate
#define  PHI 3

bool logged_windows_full = false;

adaptive_timeout_struct* init_adaptive_timeout_struct(double first_heartbeat_ms, char* node_ip) {
    adaptive_timeout_struct *metadata_struct  = (struct adaptive_timeout_struct *)malloc(sizeof *metadata_struct);
    metadata_struct->first_heartbeat_arrival_ms = first_heartbeat_ms;
    metadata_struct->freshness_point = 0;
    metadata_struct-> arrival_time_ms = 0;
    metadata_struct ->estimated_arrival_ms = 0;
    metadata_struct ->delay = INITIAL_DELAY;
    metadata_struct ->alpha = HEARTBEAT_INTERVAL;
    metadata_struct -> var = 0;
    metadata_struct -> error = 0;
    metadata_struct -> seq_num = -1;
    metadata_struct -> sum_w1 = 0L;
    metadata_struct -> sum_w2 = 0L;
    metadata_struct -> sum_time_deltas = 0L;
    metadata_struct -> past_arrival_time_differences_w1_ms = malloc(W1_WINDOW_SIZE* sizeof(double));
    metadata_struct -> past_arrival_time_differences_w2_ms = malloc(W2_WINDOW_SIZE* sizeof(double));
    metadata_struct -> deltas_between_messages = malloc(W2_WINDOW_SIZE * sizeof(double));
    strcpy(metadata_struct -> node_ip, node_ip);
    metadata_struct -> notified_windows_full = false;
    return metadata_struct;
}

void estimate_next_delay(adaptive_timeout_struct *timeout_model, double current_arrival_time, bool DEBUG) {
    double estimated_arrival_w1, estimated_arrival_w2;
    timeout_model->seq_num++;
    long long k = timeout_model->seq_num;
    current_arrival_time = current_arrival_time - timeout_model->first_heartbeat_arrival_ms;
    double time_from_last_heartbeat = current_arrival_time - timeout_model->arrival_time_ms;
    timeout_model->arrival_time_ms = current_arrival_time;
    double t = timeout_model->arrival_time_ms;



    if(DEBUG){
        printf("Time from last heartbeat: %f \n", time_from_last_heartbeat);
        printf("t: %f, k: %ld, Avg hb: %f \n", t, k, timeout_model->average_heartbeat_time_ms);
    }

//    Compute alpha using Jacobson algorithm variables
//    timeout_model->error = t - timeout_model->estimated_arrival_ms - timeout_model->delay;
//    timeout_model->delay = timeout_model->delay + GAMMA*timeout_model->error;
//    timeout_model->var = timeout_model->var + GAMMA*(fabs(timeout_model->error) - timeout_model->var);
//    timeout_model->alpha = BETA*timeout_model->delay + PHI*timeout_model->var;

    double normalized_arrival_time_diff = t - k*timeout_model->average_heartbeat_time_ms;

    if(DEBUG) {
//        printf("Error:%f Delay:%f Var:%f Alpha:%f \n", timeout_model->error,  timeout_model-> delay, timeout_model->var, timeout_model->alpha);
        printf("Normalized arrival time diff: %f\n", t - k*timeout_model->average_heartbeat_time_ms);
    }

    if(k == 0){
        timeout_model->estimated_arrival_ms = HEARTBEAT_INTERVAL;
        timeout_model->average_heartbeat_time_ms = HEARTBEAT_INTERVAL;
    }

//    Calculate sum within sliding window w1 depending on sequence number
    if(k < W1_WINDOW_SIZE){
        timeout_model->sum_w1 += normalized_arrival_time_diff;
        (*timeout_model).past_arrival_time_differences_w1_ms[k] = normalized_arrival_time_diff;
    } else {
        long past_time_index = k % W1_WINDOW_SIZE;
//        Update rolling sum
        timeout_model->sum_w1 -= timeout_model->past_arrival_time_differences_w1_ms[past_time_index];
        timeout_model->sum_w1 += normalized_arrival_time_diff;
        timeout_model->past_arrival_time_differences_w1_ms[past_time_index] = normalized_arrival_time_diff;
    }
//    Calculate sum within sliding window w2 depending on sequence number
    if(k < W2_WINDOW_SIZE){
        timeout_model->sum_w2 += normalized_arrival_time_diff;
        timeout_model->past_arrival_time_differences_w2_ms[k] = normalized_arrival_time_diff;
        timeout_model->sum_time_deltas += time_from_last_heartbeat;
        timeout_model->deltas_between_messages[k] = time_from_last_heartbeat;
    } else {
        long past_time_index = k % W2_WINDOW_SIZE;
//        Update rolling sum
        timeout_model->sum_w2 -= timeout_model->past_arrival_time_differences_w2_ms[past_time_index];
        timeout_model->sum_w2 += normalized_arrival_time_diff;
        timeout_model->past_arrival_time_differences_w2_ms[past_time_index] = normalized_arrival_time_diff;
//        Used to compute epsilon in formula
        timeout_model->sum_time_deltas -= timeout_model->deltas_between_messages[past_time_index];
        timeout_model->sum_time_deltas += time_from_last_heartbeat;
        timeout_model->deltas_between_messages[past_time_index] = time_from_last_heartbeat;
    }


    if(k != 0) {
//        Number of samples in bigger window is currently min(k, w2 window size)
        long number_of_samples_currently_in_w2 = k < W2_WINDOW_SIZE ? k : W2_WINDOW_SIZE;
        timeout_model->average_heartbeat_time_ms = timeout_model->sum_time_deltas / number_of_samples_currently_in_w2;
        estimated_arrival_w1 = (1.0/k)*timeout_model->sum_w1 + (k+1)*timeout_model->average_heartbeat_time_ms;
        estimated_arrival_w2 = (1.0/k)*timeout_model->sum_w2 + (k+1)*timeout_model->average_heartbeat_time_ms;
        timeout_model->estimated_arrival_ms =  (estimated_arrival_w1 > estimated_arrival_w2 ) ? estimated_arrival_w1 : estimated_arrival_w2;
//        printf("Sum w1:%f Sum w2:%f Avg hb time:%f\n", timeout_model->sum_w1, timeout_model->sum_w2, timeout_model->average_heartbeat_time_ms);
    }

    if(!timeout_model->notified_windows_full && k >= W1_WINDOW_SIZE && k >=W2_WINDOW_SIZE) {
        timeout_model->notified_windows_full = true;
        dzlog_info("------------------------------------------------------");
        dzlog_info("DYNAMIC TIMEOUT ALGORITHM WINDOWS FULL FOR IP %s", timeout_model->node_ip);
        dzlog_info("------------------------------------------------------");
        printf("---------------------------------------\n");
        printf("DYNAMIC TIMEOUT ALGORITHM WINDOWS FULL FOR IP %s\n", timeout_model->node_ip);
        printf("---------------------------------------\n");
    }

    timeout_model->deltas_between_messages[k % W2_WINDOW_SIZE] = time_from_last_heartbeat;
    timeout_model->freshness_point = timeout_model->estimated_arrival_ms + timeout_model->alpha;
    timeout_model->next_timeout = timeout_model->freshness_point - timeout_model->arrival_time_ms;

    if(DEBUG){
        printf("A:%f EA:%f FP:%f Timeout:%f \n", timeout_model->arrival_time_ms,  timeout_model-> estimated_arrival_ms, timeout_model->freshness_point, timeout_model->next_timeout);
    }
}

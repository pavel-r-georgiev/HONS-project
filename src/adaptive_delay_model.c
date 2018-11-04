
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "adaptive_delay_model.h"
// BETA and GAMMA determine how much weight to give past samples
#define  BETA 0.125
#define GAMMA 0.9
// Determines how much deviation from the mean to tolerate
#define  PHI 2

adaptive_timeout_struct* init_adaptive_timeout_struct(double first_heartbeat_ms) {
    adaptive_timeout_struct *metadata_struct  = (struct adaptive_timeout_struct *)malloc(sizeof *metadata_struct);
    metadata_struct->first_heartbeat_arrival_ms = first_heartbeat_ms;
    metadata_struct->freshness_delta = 0;
    metadata_struct->freshness_point = 0;
    metadata_struct-> arrival_time_ms = 0;
    metadata_struct ->estimated_arrival_ms = 0;
    metadata_struct -> u = 0;
    metadata_struct ->delay = INITIAL_DELAY;
    metadata_struct ->alpha = 0;
    metadata_struct -> var = 0;
    metadata_struct -> error = 0;
    metadata_struct -> seq_num = -1;

    return metadata_struct;
}

void estimate_next_delay(adaptive_timeout_struct *timeout_model, double current_arrival_time) {
    timeout_model->seq_num++;
    timeout_model->arrival_time_ms = current_arrival_time - timeout_model->first_heartbeat_arrival_ms;
    long k = timeout_model->seq_num;
    double t = timeout_model->arrival_time_ms;

    timeout_model->error = t - timeout_model->estimated_arrival_ms - timeout_model->alpha;
    timeout_model->delay = timeout_model->delay + GAMMA*timeout_model->error;
    timeout_model-> var = timeout_model->var + GAMMA*(fabs(timeout_model->error) - timeout_model->var);
    timeout_model->alpha = BETA*timeout_model->delay + PHI*timeout_model->var;

    if(k <= MESSAGE_COUNT_FOR_DATA_AGGREGATION){
        timeout_model->u = (t/(k+1)) * ((k*timeout_model->u)/(k+1));
        timeout_model->estimated_arrival_ms = timeout_model->u + ((k+1)/2.0)*(double)HEARTBEAT_INTERVAL;
    } else {
        long past_time_index = (k - MESSAGE_COUNT_FOR_DATA_AGGREGATION - 1) % MESSAGE_COUNT_FOR_DATA_AGGREGATION;
        double past_time =  timeout_model->past_arrival_times_ms[past_time_index];
        timeout_model->estimated_arrival_ms = timeout_model->estimated_arrival_ms + (1.0/k)*(t - past_time);
    }

    timeout_model->past_arrival_times_ms[k % MESSAGE_COUNT_FOR_DATA_AGGREGATION] = t;
    timeout_model->freshness_point = timeout_model->freshness_point + timeout_model->estimated_arrival_ms + timeout_model->alpha;

    printf("%f %f %f \n", timeout_model->arrival_time_ms, timeout_model->freshness_point, timeout_model->estimated_arrival_ms);
//    IF NODE IS SUSPECT --> REMOVE FROM SUSPECTS LIST
    if(0){
        timeout_model->freshness_delta++;
    }
}

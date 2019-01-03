
#ifndef HONS_PROJECT_ADAPTIVE_DELAY_MODEL_H
#define HONS_PROJECT_ADAPTIVE_DELAY_MODEL_H

#define INITIAL_DELAY 0
#define MESSAGE_COUNT_FOR_DATA_AGGREGATION 25
#define HEARTBEAT_INTERVAL 1000

typedef struct adaptive_timeout_struct {
    double freshness_delta;
    double freshness_point;
    double arrival_time_ms;
    double first_heartbeat_arrival_ms;
    double past_arrival_times_ms[MESSAGE_COUNT_FOR_DATA_AGGREGATION];
    double estimated_arrival_ms;
    double u;
    double delay;
    double alpha;
    double var;
    double error;
    long seq_num;
} adaptive_timeout_struct;

adaptive_timeout_struct* init_adaptive_timeout_struct(double first_heartbeat_ms);
void estimate_next_delay(adaptive_timeout_struct *heartbeat_metadata,  double current_arrival_time, FILE *fp);

#endif //HONS_PROJECT_ADAPTIVE_DELAY_MODEL_H

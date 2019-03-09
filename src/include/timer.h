#ifndef HONS_PROJECT_TIMER_H
#define HONS_PROJECT_TIMER_H
#include <stdlib.h>

typedef enum
{
    TIMER_SINGLE_SHOT = 0, /*Periodic Timer*/
    TIMER_PERIODIC         /*Single Shot Timer*/
} t_timer;

typedef void (*time_handler)(size_t timer_id, void * user_data);

int create_timer_manager();
size_t start_timer(unsigned int interval_ms, time_handler handler, t_timer type, void * user_data);
void stop_timer(size_t timer_id);
void terminate_timer_manager();

#endif //HONS_PROJECT_TIMER_H

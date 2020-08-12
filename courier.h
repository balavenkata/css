#ifndef COURIER_H
#define COURIER_H

typedef void (*time_handler)(size_t timer_id, void * user_data);

typedef struct timer_node
{
    int                 fd;
    time_handler        callback;
    void *              user_data;
    unsigned int        interval;
    struct timer_node * next;
} COURIER_TIMER_NODE;

void courier_timer_handler(size_t timer_id, void * user_data);
size_t courier_start_timer(unsigned int interval, time_handler handler, 
							void * user_data);
void courier_finalize();
void * courier_timer_thread_cb(void * data);

#endif //COURIER_H

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/timerfd.h>
#include <poll.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <glib.h>
#include <sys/timeb.h>

#include "common.h"
#include "constants.h"
#include "kitchen.h"
#include "courier.h"

//Local method (not public); init'ing the timer
static int kitchen_init_ingestion_timer(int ingestion_interval) {
    struct itimerspec new_value;
    
    int fd = timerfd_create(CLOCK_REALTIME, 0);
    if(fd != -1) {
        new_value.it_value.tv_sec = ingestion_interval / 1000;
        new_value.it_value.tv_nsec = (ingestion_interval % 1000)* 1000000;
        new_value.it_interval.tv_sec = ingestion_interval / 1000;
        new_value.it_interval.tv_nsec = (ingestion_interval %1000) * 1000000;
        timerfd_settime(fd, 0, &new_value, NULL);
    }
    return fd;
}

/**PROC+**********************************************************************/
/* Name:      kitchen_thread_cb                                              */
/*                                                                           */
/* Purpose:   This is callback function for the kitchen thread               */
/*                                                                           */
/* Returns:   Nothing, void* is for future purposes.                         */
/*                                                                           */
/*                                                                           */
/* Operation: It continuously ingests data from the input file               */
/* Upon ingesting the orders, it immediately shelves them and schedules      */
/* pickup for those orders in a random interval. If all orders have been     */
/* ingested it will wait for all deliveries to be completed before quitting  */
/*                                                                           */
/**PROC-**********************************************************************/
void *kitchen_thread_cb()
{
    int ingestion_interval, ingestion_rate;
    int fd, courier_arrive_delay, temp = 0;
    bool is_eof = false;
    time_t t;
    uint64_t ret, missed;
    char time_str_buf[64];  
    
    ////init courier thread
    size_t timer;
    //seed the random interval
    srand(time(0));  
    //the "range" is the diff between the min and max times
    //to randomize the arrival of order pickup
    const int courier_interval_range = KITCHEN_COURIER_DISPATCH_INTERVAL_MAX - 
                        KITCHEN_COURIER_DISPATCH_INTERVAL_MIN + 1;
    
    ////init ingestion
    ingestion_rate = KITCHEN_INGESTION_RATE;
    ingestion_interval = KITCHEN_INGESTION_INTERVAL;            

    fd = kitchen_init_ingestion_timer(ingestion_interval);
    if(fd == -1) {      
        current_time_msec(time_str_buf);        
        if(SYSTEM_DEBUG_LEVEL & L4) printf("%s: kitchen : L4: Cannot start kitchen thread. Quitting\n", time_str_buf);
        pthread_exit(NULL);
    }
    
    //input processing
    FILE *f = fopen(SYSTEM_ORDERS_INPUT_FILE, "r");
    if(f == NULL) {
        current_time_msec(time_str_buf);        
        if(SYSTEM_DEBUG_LEVEL & L4) printf("%s: kitchen : L4: Cannot open orders file. Quitting\n", time_str_buf);
        pthread_exit(NULL);
    }
    
    while(1) {
        current_time_msec(time_str_buf);
        if(SYSTEM_DEBUG_LEVEL & L1) printf("%s: kitchen : L1: ingestion tick\n", time_str_buf);
        
        pthread_mutex_lock(&data_access_mutex);

        //Always the LL head and tail point to valid orders (orders that 
        //have been just read OR orders that were shelved successfully).
        //On initialization (i.e. before first file read happens) they
        //shall be NULL.
        ORDER_LL_NODE *this_cycle_order = g_data->g_order_ll_tail;
        is_eof = file_read_orders(f, ingestion_rate); // g_data->g_order_ll_head & tail set 
        
        //for the first tick, take from head; else take from previous tick's tail
        this_cycle_order = (this_cycle_order == NULL ) ? 
                            g_data->g_order_ll_head : this_cycle_order;
        
        shelf_store_orders(&this_cycle_order);  //store in all hashmaps
        
        this_cycle_order = (this_cycle_order == g_data->g_order_ll_head ) ? 
                            this_cycle_order : this_cycle_order->next;
        
        //process items read in this tick; courier timer creation
        while(this_cycle_order) {
            courier_arrive_delay = (rand() % courier_interval_range) 
                                        + KITCHEN_COURIER_DISPATCH_INTERVAL_MIN;
            
            char *id_to_courier = malloc(sizeof(char) * (strlen(this_cycle_order->data->id) + 1));
            if(SYSTEM_DEBUG_LEVEL & L1) printf("%s: kitchen : L1: id_to_courier ptr %p\n", 
                        time_str_buf, id_to_courier);
            strcpy(id_to_courier, this_cycle_order->data->id);
            timer = courier_start_timer(courier_arrive_delay, 
                                courier_timer_handler, id_to_courier);
            
            current_time_msec(time_str_buf);
            if(timer) {
                if(SYSTEM_DEBUG_LEVEL & L4) printf("%s: kitchen : L4: scheduled order (%s) for pickup\n", 
                            time_str_buf, this_cycle_order->data->id);
                if(SYSTEM_DEBUG_LEVEL & L2) printf("%s: kitchen : L2: started timer (%d); courier_arrive_delay %.3f secs\n", 
                            time_str_buf, timer, courier_arrive_delay/1000.0);              
            } else {
                if(SYSTEM_DEBUG_LEVEL & L4) printf("%s: kitchen : L4: failed to schedule order (%s) for pickup\n", 
                            time_str_buf, this_cycle_order->data->id);
                //TODO: if we cannot start the courier timer, delete the order
            }
            this_cycle_order = this_cycle_order->next;
        }
        
        print_event_shelf_contents(ORDER_READ);
        
        pthread_mutex_unlock(&data_access_mutex);
        
        //items from this tick all processed; this_cycle_order should be NULL now
        if(is_eof) {
            break;
        } else {
            ret = read (fd, &missed, sizeof (missed));
        }
    }
    
    pthread_mutex_lock(&data_access_mutex);
    while(g_hash_table_size(g_data->g_order_id_shelf_hash) != 0) {              
        pthread_cond_wait(&orders_empty_cond, &data_access_mutex);              
    }
    pthread_mutex_unlock(&data_access_mutex);
    
    current_time_msec(time_str_buf);
    if(SYSTEM_DEBUG_LEVEL & L4) printf("%s: kitchen : L4: exiting\n", time_str_buf);
    
    //File close, threads exited/terminated
    fclose(f);
    courier_finalize();
    pthread_cancel(monitor_thread_id);
    pthread_join(monitor_thread_id, NULL);
    
    //when all file entries done quit/end the thread
    return 0;
}

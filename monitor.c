#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <glib.h>
#include <sys/timeb.h>

#include "common.h"
#include "constants.h"

//Not a public method; initing the monitor thread timer
static int monitor_init_ingestion_timer(int shelf_monitor_interval) {
    struct itimerspec new_value;
    
    int fd = timerfd_create(CLOCK_REALTIME, 0);
    if(fd != -1) {
        new_value.it_value.tv_sec = shelf_monitor_interval / 1000;
        new_value.it_value.tv_nsec = (shelf_monitor_interval % 1000)* 1000000;
        new_value.it_interval.tv_sec = shelf_monitor_interval / 1000;
        new_value.it_interval.tv_nsec = (shelf_monitor_interval %1000) * 1000000;
        timerfd_settime(fd, 0, &new_value, NULL);
    }
    return fd;
}

/**PROC+**********************************************************************/
/* Name:      monitor_check_remove_stale_order                               */
/*                                                                           */
/* Purpose:   To handle order stalenss (as per "Shelf Life" section in       */
/*            problem statement.                                             */
/*                                                                           */
/* Params:    IN     shelf           - Shelf where the order sits now        */
/*            IN     order           - Order to check for staleness          */
/*            IN     elapsed_time    - Time elapsed (in milliseconds) since  */
/*                                     order ingestion                       */
/*                                                                           */
/* Returns:   bool - for success/failure.                                    */
/*                                                                           */
/*                                                                           */
/* Operation: Check if order is stale (based on age and "value" logic per    */
/* problem statement). If stale discard.                                     */
/*                                                                           */
/**PROC-**********************************************************************/
bool monitor_check_remove_stale_order(SHELF shelf, ORDER *order, int elapsed_time) {
    char time_str_buf[64];
    bool is_removed = false;
    double value;
    
    int shelfDecayModifier = (shelf == OVERFLOW_SHELF) ? 
                                SHELF_LIFE_MODIFIER_OVERFLOW_SHELF : SHELF_LIFE_MODIFIER_SINGLE_TEMP_SHELF;
    
    current_time_msec(time_str_buf);
    value = order->shelfLife - (order->decayRate * (elapsed_time/1000) * shelfDecayModifier);
    if(SYSTEM_DEBUG_LEVEL & L1) printf("%s: monitor : L1: order id %s order value %f...\n", time_str_buf, order->id, value);
    if(value < 0) {
        //remove order
        if(SYSTEM_DEBUG_LEVEL & L4) printf("%s: monitor : L4: order id %s is STALE; removing\n", time_str_buf, order->id);
        
        //TODO: remove all references to order
        //Remove from all hashes, free heap memory of members and finally free order's heap memory
        int *ptr_shelf = g_hash_table_lookup(g_data->g_order_id_shelf_hash, order->id);
        free(ptr_shelf);
        g_hash_table_remove(g_data->g_order_id_shelf_hash, order->id);
        
        if(shelf == OVERFLOW_SHELF) {
            ORDER** overflow_by_temp = g_data->g_overflow_by_temp_array[order->temp];
            
            int i = OVERFLOW_SHELF_MAX_SIZE-1, order_idx = -1;
            ORDER *swap = NULL;
            for(i = 0; i < OVERFLOW_SHELF_MAX_SIZE ; i++) {
                if(swap == order) {
                    overflow_by_temp[i] = NULL;
                    g_data->g_overflow_by_temp_array_sz[order->temp]--;
                    break;
                }
            }
        }
        
        //Caller will do using the iterator
        //GHashTable *shelf_hash = shelf_to_hash(shelf);
        //g_hash_table_remove(shelf_hash, order->id);       
        
        if(SYSTEM_DEBUG_LEVEL & L1) printf("%s: monitor : L1: FREE order->id %p order->name %p order %p\n", 
                        time_str_buf, order->id, order->name, order);
        free(order->id);
        free(order->name);
        free(order);
        
        is_removed = true;
    }
    
    return is_removed;
}

/**PROC+**********************************************************************/
/* Name:      monitor_thread_cb                                              */
/*                                                                           */
/* Purpose:   Callback for the monitor thread; invoked at configured         */
/*            periodicity                                                    */
/*            problem statement.                                             */
/*                                                                           */
/* Returns:   void* - Not used for now.                                      */
/*                                                                           */
/*                                                                           */
/* Operation: SEe "Purpose" above                                            */
/*                                                                           */
/**PROC-**********************************************************************/
void *monitor_thread_cb() {
    int shelf_monitor_interval = SHELF_MONITOR_INTERVAL;
    int fd = monitor_init_ingestion_timer(shelf_monitor_interval);
    uint64_t ret, missed;
    char time_str_buf[64];
    SHELF shelf_iter;
    GHashTable *shelf_hash;
    GHashTableIter shelf_hash_iter;
    gpointer key_order_id, value_order;
    char *order_id;
    ORDER *order;
    struct timeb monitor_time;
    int diff; //msecs
    
    if(fd == -1) {      
        current_time_msec(time_str_buf);        
        if(SYSTEM_DEBUG_LEVEL & L4) printf("%s: monitor : L4: Cannot start shelf monitor thread. Quitting\n", time_str_buf);
        pthread_exit(NULL);
    }
        
    while(1) {
        current_time_msec(time_str_buf);
        if(SYSTEM_DEBUG_LEVEL & L1) printf("%s: monitor : L1: shelf monitor tick\n", time_str_buf);
        ftime(&monitor_time);
        
        pthread_mutex_lock(&data_access_mutex);
        for(shelf_iter = HOT_SHELF; (shelf_iter < MAX_SHELF); shelf_iter++) {
            shelf_hash = shelf_to_hash(shelf_iter);
            
            if(shelf_hash) {
                g_hash_table_iter_init(&shelf_hash_iter, shelf_hash);
                while (g_hash_table_iter_next(&shelf_hash_iter, &key_order_id, &value_order)) {
                    order_id = (char*)key_order_id;
                    order = (ORDER*)value_order;
                    diff = (1000.0 * (monitor_time.time - order->creationTime.time) + 
                                            (monitor_time.millitm - order->creationTime.millitm));
                                        
                    //printf("%s: monitor : L1: order_id  %s order %p name %s diff %u\n", time_str_buf, 
                    //                          order_id, order, order->name, diff);
                    if(monitor_check_remove_stale_order(shelf_iter, order, diff)) {
                        //Item removed since stale, remove from shelf hash too
                        g_hash_table_iter_remove(&shelf_hash_iter);
                        print_event_shelf_contents(ORDER_DISCARDED_STALE);
                    }
                }
            }
        }
        pthread_mutex_unlock(&data_access_mutex);
        
        ret = read (fd, &missed, sizeof (missed));
    }
    
    return 0;
}
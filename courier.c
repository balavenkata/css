#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/timerfd.h>
#include <poll.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <glib.h>
#include <sys/timeb.h>

#include "common.h"
#include "constants.h"
#include "courier.h"

//TODO: hardcoded timer limit; revisit
#define MAX_TIMER_COUNT 1000
static COURIER_TIMER_NODE *g_head = NULL;

/**PROC+**********************************************************************/
/* Name:      courier_timer_handler                                          */
/*                                                                           */
/* Purpose:   This models the "courier" or the pickup person who picks up    */
/*            an order that is ready to be delivered                         */
/*                                                                           */
/* Returns:   Nothing.                                                       */
/*                                                                           */
/* Params:    IN     timer_id   - ID for the timer representing "this"       */
/*                                instance of the courier                    */
/*            IN     user_data  - ID for the order to be delivered.          */
/*                                                                           */
/* Operation: Looks up the high level hash first that gives the shelf.       */
/*            Then it goes pulls the order out of the specific shelf         */
/*            Finally it does house cleaning (removing the oeder from system)*/
/*                                                                           */
/**PROC-**********************************************************************/
void courier_timer_handler(size_t timer_id, void *user_data)
{
    char time_str_buf[64];
    current_time_msec(time_str_buf);
    
    char *order_id = (char*)user_data;
    if(SYSTEM_DEBUG_LEVEL & L3) printf("%s: courier : L3: timer (%d); order_id %s \n",  
                time_str_buf, timer_id, order_id);
    
    pthread_mutex_lock(&data_access_mutex);
    
    int *ptr_shelf = g_hash_table_lookup(g_data->g_order_id_shelf_hash, order_id);
    if(ptr_shelf) {
        SHELF shelf = (SHELF)(*ptr_shelf);
        GHashTable *shelf_hash = shelf_to_hash(shelf);
        ORDER *order = g_hash_table_lookup(shelf_hash, order_id);
        
        if(order) {
            if(SYSTEM_DEBUG_LEVEL & L3) printf("%s: courier : L3: timer (%d); order_name %s \n", 
                            time_str_buf, timer_id, order->name);
            
            g_hash_table_remove(shelf_hash, order_id);
            g_hash_table_remove(g_data->g_order_id_shelf_hash, order_id);
            
            if(shelf==OVERFLOW_SHELF) {
                if(SYSTEM_DEBUG_LEVEL & L3) printf("%s: courier : L3: removing from overflow shelf order id %s \n",  
                            time_str_buf, order->id);
                ORDER** overflow_by_temp = g_data->g_overflow_by_temp_array[order->temp];
                int i = OVERFLOW_SHELF_MAX_SIZE-1, order_idx = -1;
                ORDER *swap = NULL;
                for(; i >=0 ; i--) {
                    if(overflow_by_temp[i] == order) {
                        if(swap) {
                            order_idx = i;
                        }
                        break;
                    } else {                        
                        if(order_idx == -1 && !swap && overflow_by_temp[i]) {
                            //printf("$$$$ %p..\n", overflow_by_temp[i]);
                            swap = (ORDER*)(overflow_by_temp[i]);                           
                        }                           
                    }
                }
                if(SYSTEM_DEBUG_LEVEL & L3) printf("%s: courier : L3: swap id is %s order_idx %d\n",  
                            time_str_buf, (swap==NULL) ? "NONE" : swap->id, order_idx);
                
                if(swap && order_idx > -1) {
                    overflow_by_temp[order_idx] = swap; 
                    //printf("$$$$ order_idx %d swap->id %s swap->name %s..\n", 
                    //           order_idx, swap->id, swap->name);
                }                   
                //Either the removed order is the last one(so set to NULL); or it has 
                //been swapped with last one - see above if case(so swap to NULL
                overflow_by_temp[g_data->g_overflow_by_temp_array_sz[order->temp]-1] = NULL;                
                g_data->g_overflow_by_temp_array_sz[order->temp]--;
                if(SYSTEM_DEBUG_LEVEL & L3) printf("%s: courier : L3: order->id is %s overflow by temp array sz %d\n",  
                            time_str_buf, order->id, 
                            g_data->g_overflow_by_temp_array_sz[order->temp]);
            }
            
            if(SYSTEM_DEBUG_LEVEL & L1) printf("%s: courier : L1: order_id %p order %p shelf %s...\n", 
                        time_str_buf, order_id, order, ordershelf_to_str(shelf));
            if(SYSTEM_DEBUG_LEVEL & L4) printf("%s: courier : L4: order_id %s successfully delivered\n", 
                        time_str_buf, order_id);
            print_event_shelf_contents(ORDER_DELIVERED);
            free(order_id);
            
            //TODO- do all order free related tasks in one place  
            if(SYSTEM_DEBUG_LEVEL & L1) printf("%s: courier : L1: FREE order->id %p order->name %p order %p\n", 
                        time_str_buf, order->id, order->name, order);
            free(ptr_shelf);
            free(order->id);
            free(order->name);
            free(order);            
        } else {
            if(SYSTEM_DEBUG_LEVEL & L3) printf("%s: courier : L3: order_id %s shelf %s is not in any hash\n",  
                        time_str_buf, order_id, ordershelf_to_str(shelf));
            free(order_id);
            free(ptr_shelf);
        }
    } else {
        if(SYSTEM_DEBUG_LEVEL & L4) 
             printf("%s: courier : L4: order_id %s shelf not found (possibly removed by monitor as stale)\n",  
                    time_str_buf, order_id);
        free(order_id);
    }
    
    pthread_mutex_unlock(&data_access_mutex);
}

/**PROC+**********************************************************************/
/* Name:      courier_start_timer                                            */
/*                                                                           */
/* Purpose:   Creates a timer node to track one order's delivery             */
/*                                                                           */
/* Returns:   Nothing.                                                       */
/*                                                                           */
/* Params:    IN     interval   - Random interval to schedule delivery       */
/*            IN     handler    - Represents the callback function for the   */
/*                                timer                                      */
/*            IN     user_data  - ID for the order to be delivered.          */
/*                                                                           */
/* Operation: Inserts a timer node for the courier delivery                  */
/*                                                                           */
/**PROC-**********************************************************************/
size_t courier_start_timer(unsigned int interval, time_handler handler, 
                            void * user_data)
{
    COURIER_TIMER_NODE * new_node = NULL;
    struct itimerspec new_value;

    new_node = (COURIER_TIMER_NODE *)malloc(sizeof(COURIER_TIMER_NODE));
    //printf("%s: kitchen : L1: new_node ptr %p\n", "courier_start_timer", new_node);

    if(new_node == NULL) 
        return 0;

    new_node->callback  = handler;
    new_node->user_data = user_data;
    new_node->interval  = interval;

    new_node->fd = timerfd_create(CLOCK_REALTIME, 0);

    if (new_node->fd == -1) {
        free(new_node);
        return 0;
    }
   
    new_value.it_value.tv_sec = interval / 1000;
    new_value.it_value.tv_nsec = (interval % 1000)* 1000000;
    new_value.it_interval.tv_sec= 0;  //always our courier timers are one-shot; not periodic
    new_value.it_interval.tv_nsec = 0; //always our courier timers are one-shot; not periodic

    timerfd_settime(new_node->fd, 0, &new_value, NULL);

    /*Inserting the timer node into the list*/
    new_node->next = g_head;
    g_head = new_node;

    return (size_t)new_node;
}

//Not a 'public' function; only internal to this file.
static void courier_stop_timer(size_t timer_id)
{
    COURIER_TIMER_NODE * tmp = NULL;
    COURIER_TIMER_NODE * node = (COURIER_TIMER_NODE *)timer_id;

    if (node == NULL) return;

    close(node->fd);

    if(node == g_head)
    {
        g_head = g_head->next;
    } else {

        tmp = g_head;

        while(tmp && tmp->next != node) tmp = tmp->next;

        if(tmp)
        {
            /*tmp->next can not be NULL here.*/
            tmp->next = tmp->next->next;
        }
    }
    if(node) free(node);
}

/**PROC+**********************************************************************/
/* Name:      courier_finalize                                               */
/*                                                                           */
/* Purpose:   This is used to cancel the courier thread when orders have     */
/*            read and delivered                                             */
/*                                                                           */
/* Returns:   Nothing.                                                       */
/*                                                                           */
/*                                                                           */
/* Operation: See"Purpose" above                                             */
/*                                                                           */
/**PROC-**********************************************************************/
void courier_finalize()
{
    while(g_head) courier_stop_timer((size_t)g_head);

    pthread_cancel(courier_thread_id);
    pthread_join(courier_thread_id, NULL);
}

//Not a 'public' function; only internal to this file.
static COURIER_TIMER_NODE *courier_get_timer_from_fd(int fd)
{
    COURIER_TIMER_NODE * tmp = g_head;
    
    while(tmp)
    {
        if(tmp->fd == fd) return tmp;

        tmp = tmp->next;
    }
    return NULL;
}

/**PROC+**********************************************************************/
/* Name:      courier_timer_thread_cb                                        */
/*                                                                           */
/* Purpose:   This is callback function for the courier thread               */
/*                                                                           */
/* Returns:   Nothing, void* is for future purposes.                         */
/*                                                                           */
/*                                                                           */
/* Operation: It continuously checks if any timer file descriptor (fd) is    */
/* set using poll() system call. If any timer file descriptor is set and     */
/* readable using read() system call, then it calls the callback function of */
/* the timer with timer id and user_data.                                    */
/*                                                                           */
/* Here the callback is nothing but the courier delivery function            */
/* When all orders are delivered, it send a signal to Kitchen thread who     */
/* is waiting to quit                                                        */
/*                                                                           */
/**PROC-**********************************************************************/
void *courier_timer_thread_cb(void * data)
{
    struct pollfd ufds[MAX_TIMER_COUNT] = {{0}};
    int iMaxCount = 0;
    COURIER_TIMER_NODE * tmp = NULL;
    int read_fds = 0, i, s;
    uint64_t exp;
    time_t t;

    while(1)
    {
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        pthread_testcancel();
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

        iMaxCount = 0;
        tmp = g_head;

        memset(ufds, 0, sizeof(struct pollfd)*MAX_TIMER_COUNT);
        while(tmp && iMaxCount < MAX_TIMER_COUNT)
        {
            ufds[iMaxCount].fd = tmp->fd;
            ufds[iMaxCount].events = POLLIN;
            iMaxCount++;

            tmp = tmp->next;
        }
        read_fds = poll(ufds, iMaxCount, 100);

        if (read_fds <= 0) continue;

        for (i = 0; i < iMaxCount; i++)
        {
            if (ufds[i].revents & POLLIN)
            {
                s = read(ufds[i].fd, &exp, sizeof(uint64_t));

                if (s != sizeof(uint64_t)) continue;

                tmp = courier_get_timer_from_fd(ufds[i].fd);

                if(tmp && tmp->callback) tmp->callback((size_t)tmp, tmp->user_data);

                //Since the job of courier is done, remove the timer node
                courier_stop_timer((size_t)tmp);
            }
            
            //If all orders have been delivered send a signal to kitchen thread
            pthread_mutex_lock(&data_access_mutex);
            if(g_hash_table_size(g_data->g_order_id_shelf_hash) == 0) { 
                pthread_cond_signal(&orders_empty_cond);                
            }
            pthread_mutex_unlock(&data_access_mutex);
        }
    }
    
    char time_str_buf[64];
    current_time_msec(time_str_buf);        
    if(SYSTEM_DEBUG_LEVEL & L1) printf("%s: courier : L1: exiting\n", time_str_buf);

    return NULL;
}

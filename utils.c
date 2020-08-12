#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <glib.h>
#include <sys/timeb.h>

#include "common.h"
#include "constants.h"
#include "kitchen.h"

/**PROC+**********************************************************************/
/* Name:      init                                                           */
/*                                                                           */
/* Purpose:   To init all global data structures that hold in-mem state      */
/*                                                                           */
/* Returns:   bool - for success/failure.                                     */
/*                                                                           */
/*                                                                           */
/* Operation: System init of all key data structures (hashtables mostly)     */
/*                                                                           */
/**PROC-**********************************************************************/
bool init() {
    bool init_success = true;
    char time_str_buf[64];  
    
    init_success = read_properties();
    current_time_msec(time_str_buf);    
    g_data = malloc(sizeof(DATA));
    if(SYSTEM_DEBUG_LEVEL & L1) printf("%s: input   : L1: g_data ptr %p\n", time_str_buf, g_data);
    if(g_data) {
        g_data->g_order_ll_head = NULL;
        g_data->g_order_ll_tail = NULL;
        
        g_data->g_order_id_shelf_hash = g_hash_table_new(g_str_hash, g_str_equal);
        g_data->g_order_id_hot_shelf_hash = g_hash_table_new(g_str_hash, g_str_equal);
        g_data->g_order_id_cold_shelf_hash = g_hash_table_new(g_str_hash, g_str_equal);
        g_data->g_order_id_frozen_shelf_hash = g_hash_table_new(g_str_hash, g_str_equal);
        g_data->g_order_id_overflow_shelf_hash = g_hash_table_new(g_str_hash, g_str_equal);
        
        g_data->g_overflow_by_temp_array = (ORDER***)malloc(MAX_TEMP*sizeof(ORDER*));
        g_data->g_overflow_by_temp_array_sz = malloc(MAX_TEMP*sizeof(int)); 

        if( g_data->g_order_id_shelf_hash == NULL || g_data->g_order_id_hot_shelf_hash == NULL ||
            g_data->g_order_id_cold_shelf_hash == NULL || g_data->g_order_id_frozen_shelf_hash == NULL ||
            g_data->g_order_id_overflow_shelf_hash == NULL ||
            g_data->g_overflow_by_temp_array == NULL || g_data->g_overflow_by_temp_array_sz == NULL
          ) {
              init_success = false;
        } else { 
            //All mem alloc's fine so far...continue.
            TEMP t;
            for(t = HOT; t < MAX_TEMP; t++) {
                g_data->g_overflow_by_temp_array[t] = 
                                (ORDER**)malloc(OVERFLOW_SHELF_MAX_SIZE * sizeof(ORDER*));
                memset(g_data->g_overflow_by_temp_array[t], 0, 
                                OVERFLOW_SHELF_MAX_SIZE*sizeof(ORDER*));
                g_data->g_overflow_by_temp_array_sz[t] = 0;
            }
        }
        
    } else {
        init_success = false;
    }
    
    return init_success;
}

/**PROC+**********************************************************************/
/* Name:      finalize                                                       */
/*                                                                           */
/* Purpose:   To free all global data structures that hold in-mem state      */
/*                                                                           */
/* Returns:   None.                                                          */
/*                                                                           */
/*                                                                           */
/* Operation: System free of all key data structures                         */
/*                                                                           */
/**PROC-**********************************************************************/
void finalize() {   
    GHashTableIter iter;
    gpointer key_order_id, value_shelf_enum;
    g_hash_table_iter_init(&iter, g_data->g_order_id_shelf_hash);
    while (g_hash_table_iter_next (&iter, &key_order_id, &value_shelf_enum)) {
        free(value_shelf_enum);
    }
    g_hash_table_destroy(g_data->g_order_id_shelf_hash);
    
    g_hash_table_destroy(g_data->g_order_id_hot_shelf_hash);
    g_hash_table_destroy(g_data->g_order_id_cold_shelf_hash);
    g_hash_table_destroy(g_data->g_order_id_frozen_shelf_hash);
    g_hash_table_destroy(g_data->g_order_id_overflow_shelf_hash);
    
    TEMP t;
    for(t = HOT; t < MAX_TEMP; t++) {
        free(g_data->g_overflow_by_temp_array[t]);      
    }
    free(g_data->g_overflow_by_temp_array);
    free(g_data->g_overflow_by_temp_array_sz);
    
    //TODO - g_order_ll_head, tail free must happen sooner ? Or we can keep this around 
    //to count items processed
    int i = 0;
    ORDER_LL_NODE *ll_node_to_free = g_data->g_order_ll_head, *next;
    while(ll_node_to_free) {
        i++;
        next = ll_node_to_free->next;
        free(ll_node_to_free);
        ll_node_to_free = next;
    }
    if(SYSTEM_DEBUG_LEVEL & L1) printf("%s: input   : L1: LL size %d\n", "finalize", i);
    
    free(g_data);
    free(SYSTEM_ORDERS_INPUT_FILE);
}

/**PROC+**********************************************************************/
/* Name:      current_time_msec                                              */
/*                                                                           */
/* Purpose:   Given a buffer this method fills it with current system date   */
/*                                                                           */
/* Params:    IN/OUT      buf     buffer passed by caller is filled in       */
/*                                                                           */
/* Returns:   None.                                                          */
/*                                                                           */
/*                                                                           */
/* Operation: Buffer is filled with  formatted date/time string              */
/*                                                                           */
/**PROC-**********************************************************************/
void current_time_msec(char *buf) {
    struct timeval tv;
    time_t nowtime;
    struct tm *nowtm;
    char tmbuf[64];

    gettimeofday(&tv, NULL);

    nowtime = tv.tv_sec;
    nowtm = localtime(&nowtime);
    strftime(tmbuf, sizeof tmbuf, "%Y-%m-%d %H:%M:%S", nowtm);
    snprintf(buf, 64, "%s.%03d", tmbuf, tv.tv_usec/1000);            
}

//Self explanatory util method...returns max size of shelves
int ordershelf_to_max_size(SHELF shelf) {
    int shelf_size = 0;
    switch(shelf) {
        case HOT_SHELF:
        shelf_size = HOT_SHELF_MAX_SIZE;
        break;
        case COLD_SHELF:
        shelf_size = COLD_SHELF_MAX_SIZE;
        break;
        case FROZEN_SHELF:
        shelf_size = FROZEN_SHELF_MAX_SIZE;
        break;
        case OVERFLOW_SHELF:
        shelf_size = OVERFLOW_SHELF_MAX_SIZE;
        break;
        default:
        break;
    }
    
    return shelf_size;
}

//Self explanatory util method...returns string for display
char *ordertemp_to_str(TEMP temp) {
    switch(temp) {
        case HOT:
            return "HOT";
            break;
        case COLD:
            return "COLD";
            break;
        case FROZEN:
            return "FROZEN";
            break;
        default:
            return "Undefined";
            break;
    }
}

//Self explanatory util method...returns string for display
char *ordershelf_to_str(SHELF shelf) {
    switch(shelf) {
        case HOT_SHELF:
            return "HOT_SHELF";
            break;
        case COLD_SHELF:
            return "COLD_SHELF";
            break;
        case FROZEN_SHELF:
            return "FROZEN_SHELF";
            break;
        case OVERFLOW_SHELF:
            return "OVERFLOW_SHELF";
            break;
        default:
            return "Undefined";
            break;
    }
}

//Self explanatory util method...returns string for display
char *order_event_to_str(ORDER_EVENT evt) {
    switch(evt) {
        case ORDER_READ:
            return "ORDER_READ";
            break;
        case ORDER_DELIVERED:
            return "ORDER_DELIVERED";
            break;
        case ORDER_DISCARDED_SHELF_FULL:
            return "ORDER_DISCARDED_SHELF_FULL";
            break;
        case ORDER_DISCARDED_STALE:
            return "ORDER_DISCARDED_STALE";
            break;
        default:
            return "UNKNOWN";
            break;
    }
}

//Print formatted detailed output as per problem statement
//Also prints "value" of the order calculated using age of the order
static void print_order_contents(ORDER *order, double value) {
    char buffer[20];
    
    printf("\t{\n");
    printf("\t\t\"id\": \"%s\",\n", order->id);
    printf("\t\t\"name\": \"%s\",\n", order->name);
    printf("\t\t\"temp\": \"%s\",\n", ordertemp_to_str(order->temp));
    sprintf(buffer,"%d",order->shelfLife);
    printf("\t\t\"shelfLife\": \"%s\",\n", buffer);
    sprintf(buffer,"%f",order->decayRate);
    printf("\t\t\"decayRate\": \"%s\",\n", buffer);
    sprintf(buffer,"%f",value);
    printf("\t\t\"value\": \"%s\",\n", buffer);
    printf("\t}");
}

//Print formatted detailed output as per problem statement on key events
void print_event_shelf_contents(ORDER_EVENT evt) {
    if(SYSTEM_PRINT_SHELF_CONTENTS) {
        SHELF shelf_iter;
        int elapsed_time;//seconds
        double value;
        bool is_first;
        GHashTable *shelf_hash;
        GHashTableIter shelf_hash_iter;
        gpointer key_order_id, value_order;
        ORDER *order;
        struct timeb print_time;
        char time_str_buf[64];  
        
        printf("-------------------------------\n");
        current_time_msec(time_str_buf);
        printf("TIMESTAMP: %s\n", time_str_buf);
        printf("EVENT: %s\n", order_event_to_str(evt));
        
        ftime(&print_time);
        int shelfDecayModifier;
                
        for(shelf_iter = HOT_SHELF; (shelf_iter < MAX_SHELF); shelf_iter++) {
                shelf_hash = shelf_to_hash(shelf_iter);
                printf("SHELF: [%s]\n", ordershelf_to_str(shelf_iter));
                printf("CONTENTS:[");
                is_first = true;
                
                if(shelf_hash) {
                    shelfDecayModifier = (shelf_iter == OVERFLOW_SHELF) ? 
                                    SHELF_LIFE_MODIFIER_OVERFLOW_SHELF : SHELF_LIFE_MODIFIER_SINGLE_TEMP_SHELF;
                    g_hash_table_iter_init(&shelf_hash_iter, shelf_hash);
                    while (g_hash_table_iter_next(&shelf_hash_iter, &key_order_id, &value_order)) {
                        printf("%s", is_first ? "\n" : ",\n");
                        if(is_first) is_first = false;
                        order = (ORDER*)value_order;
                        elapsed_time = (1000.0 * (print_time.time - order->creationTime.time) + 
                                                (print_time.millitm - order->creationTime.millitm));
                                            
                        //printf("%s: monitor : L1: order_id  %s order %p name %s diff %u\n", time_str_buf, 
                        //                          order_id, order, order->name, diff);
                        value = order->shelfLife - (order->decayRate * (elapsed_time/1000) * shelfDecayModifier);
                        print_order_contents(order, value);
                    }
                }
                printf("%s]\n", is_first ? "": "\n");
            }
        printf("-------------------------------\n");
    }//if(SYSTEM_PRINT_SHELF_CONTENTS)
}

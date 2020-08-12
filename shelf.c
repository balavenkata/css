#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <glib.h>
#include <sys/timeb.h>

#include "common.h"
#include "constants.h"
#include "kitchen.h"

GHashTable *shelf_to_hash(SHELF shelf) {
    GHashTable *shelf_hash = (shelf==HOT_SHELF) ? g_data->g_order_id_hot_shelf_hash :
                                ((shelf==COLD_SHELF)  ? g_data->g_order_id_cold_shelf_hash :
                                    ((shelf==FROZEN_SHELF)  ? g_data->g_order_id_frozen_shelf_hash : 
                                        ((shelf==OVERFLOW_SHELF)  ? g_data->g_order_id_overflow_shelf_hash : NULL)));
    return shelf_hash;
}

//Internal method but key logic is here for shelving orders
//It goes as follows
//      - if shelf space is there for matching heat order, then it stores in the shelf
//      - else if shelf space is there in overflow shelf, then it stores in the shelf
//      - else if some order in overflow can be moved to its "matching heat shelf" it does that
//      - else...
//              problem statement calls for selecting something in overflow at random and discard
//              logic in this function simply discards the new order.
static bool shelf_place_order_in_shelf(ORDER *order, SHELF *shelf, int shelf_size) {
    TEMP temp_iter;
    char time_str_buf[64];
    bool order_shelved_success = true;
    
    GHashTable *shelf_hash = shelf_to_hash(*shelf);
    
    if(g_hash_table_size(shelf_hash) < shelf_size) {
        g_hash_table_insert(shelf_hash, order->id, order);
    } else if (g_hash_table_size(g_data->g_order_id_overflow_shelf_hash) < OVERFLOW_SHELF_MAX_SIZE) { 
        current_time_msec(time_str_buf);
        if(SYSTEM_DEBUG_LEVEL & L2) printf("%s: shelf   : L2: OVERFLOW SIZE %d\n", time_str_buf, 
                    g_hash_table_size(g_data->g_order_id_overflow_shelf_hash));
        
        g_hash_table_insert(g_data->g_order_id_overflow_shelf_hash, order->id, order);
        *shelf = OVERFLOW_SHELF;
        if(SYSTEM_DEBUG_LEVEL & L2) printf("%s: shelf   : L2: order id %s temp %s\n", time_str_buf, order->id, "MOVE TO OVERFLOW"); 
        
        if(SYSTEM_DEBUG_LEVEL & L2) printf("%s: shelf   : L2: overflow-temp-arr-sz is %d temp %s; order is %p\n", 
                    time_str_buf, g_data->g_overflow_by_temp_array_sz[order->temp], 
                    ordertemp_to_str(order->temp), order);
                                    
        ORDER** overflow_by_temp = g_data->g_overflow_by_temp_array[order->temp];
        overflow_by_temp[g_data->g_overflow_by_temp_array_sz[order->temp]] = order;
        
        g_data->g_overflow_by_temp_array_sz[order->temp]++;
    }else {
        //can we move items from overflow to a free shelf ?
        bool moved_from_overflow = false;
        GHashTable *other_shelf_hash; //the hash/shelf to move the item from overflow to
        int other_shelf_max_sz, overflow_by_temp_sz;
        ORDER *moved_order;
        
        current_time_msec(time_str_buf);        
        for(temp_iter = HOT; (temp_iter < MAX_TEMP && !moved_from_overflow); temp_iter++) {
            overflow_by_temp_sz = g_data->g_overflow_by_temp_array_sz[temp_iter];
            if(overflow_by_temp_sz > 0) {
                other_shelf_hash = shelf_to_hash(temp_iter); //using temperature as shelf
                other_shelf_max_sz = ordershelf_to_max_size(temp_iter);
                if(g_hash_table_size(other_shelf_hash) < other_shelf_max_sz) {
                    //this shelf has space
                    
                    //Step 1: moved item back to its single-temperature shelf
                    moved_order = g_data->g_overflow_by_temp_array[temp_iter][overflow_by_temp_sz-1];
                    
                    //order removed from OVERFLOW shelf
                    g_hash_table_remove(g_data->g_order_id_overflow_shelf_hash, moved_order->id);
                    g_data->g_overflow_by_temp_array[temp_iter][overflow_by_temp_sz-1] = NULL;
                    //order removed also from the overlow-sz-array
                    g_data->g_overflow_by_temp_array_sz[temp_iter]--; 
                    
                    g_hash_table_insert(other_shelf_hash, moved_order->id, moved_order);
                    int *ptr_shelf = g_hash_table_lookup(g_data->g_order_id_shelf_hash, moved_order->id);
                    *ptr_shelf = (int)temp_iter; //using temperature as shelf
                    
                    if(SYSTEM_DEBUG_LEVEL & L1) printf("%s: shelf   : L1: moved order temp array sz %d temp %s...\n", 
                                time_str_buf, g_data->g_overflow_by_temp_array_sz[temp_iter], 
                                ordertemp_to_str(temp_iter));
                    
                    //Step 2: now add new item to the overflow shelf
                    //new order inserted into overflow
                    g_hash_table_insert(g_data->g_order_id_overflow_shelf_hash, order->id, order);
                    *shelf = OVERFLOW_SHELF;
                    ORDER** overflow_by_temp = g_data->g_overflow_by_temp_array[order->temp];
                    //new order inserted also to the overlow-sz-array
                    overflow_by_temp[g_data->g_overflow_by_temp_array_sz[order->temp]] = order; 
                    g_data->g_overflow_by_temp_array_sz[order->temp]++;
                    
                    if(SYSTEM_DEBUG_LEVEL & L1) printf("%s: shelf   : L1: new order temp array sz %d temp %s...\n", time_str_buf, 
                                    g_data->g_overflow_by_temp_array_sz[order->temp], 
                                    ordertemp_to_str(order->temp));
                    if(SYSTEM_DEBUG_LEVEL & L1) printf("%s: shelf   : L1: moving order id %s from OVERFLOW to temp %s...\n", 
                                    time_str_buf, moved_order->id, ordertemp_to_str(temp_iter));
                    if(SYSTEM_DEBUG_LEVEL & L1) printf("%s: shelf   : L1: moved hash size %d overflow hash size %d other_shelf_max_sz %d...\n", 
                                    time_str_buf, g_hash_table_size(other_shelf_hash), 
                                    g_hash_table_size(g_data->g_order_id_overflow_shelf_hash), 
                                    other_shelf_max_sz);
                    
                    moved_from_overflow = true;
                } else {
                    if(SYSTEM_DEBUG_LEVEL & L1) printf("%s: shelf   : L1: overflow_by_temp_sz %d other_shelf_hash %d...\n", 
                                    time_str_buf, overflow_by_temp_sz, g_hash_table_size(other_shelf_hash));
                    if(SYSTEM_DEBUG_LEVEL & L1) printf("%s: shelf   : L1: shelf %s is FULL for order->id %s...\n", 
                                    time_str_buf, ordertemp_to_str(temp_iter), order->id);
                }
            } else {
                if(SYSTEM_DEBUG_LEVEL & L1) printf("%s: shelf   : L1: overflow_by_temp_sz is ZERO %s...\n", 
                                    time_str_buf, ordertemp_to_str(temp_iter));
            }               
        }
        
        if(!moved_from_overflow) {
            //the order is dropped; 
            if(SYSTEM_DEBUG_LEVEL & L1) printf("%s: shelf   : L1: order->id %s order %p order->id %p could NOT be shelved; it will be dropped\n", 
                                    time_str_buf, order->id, order, order->id);
            if(SYSTEM_DEBUG_LEVEL & L4) printf("%s: shelf   : L4: order->id %s will be dropped\n", time_str_buf, order->id);
            
            //TODO: instead of taking an existing order at random, why not drop the new order itself ?
            //      Revisit this logic and confirm
            
            //free(order); //done in shelf_store_orders()
            order_shelved_success = false;
        } else {
            *shelf = OVERFLOW_SHELF;
            //also update the moved item's new shelf
        }
    }

    return order_shelved_success;
}

/**PROC+**********************************************************************/
/* Name:      shelf_store_orders                                             */
/*                                                                           */
/* Purpose:   To handle order stalenss (as per "Shelf Life" section in       */
/*            problem statement.                                             */
/*                                                                           */
/* Params:    IN/OUT   this_cycle_order  - In the global LL this is the      */
/*                                         head of the subset to shelf in    */
/*                                         "this" cycle                      */
/*                                                                           */
/* Returns:   None                                                           */
/*                                                                           */
/*                                                                           */
/* Operation: Iterate thru the portion of LL (linked list), shelf them in    */
/*            the order which they were read in. In the process if the LL    */
/*            node is freed, adjust the global LL head/tail accordingly      */
/*                                                                           */
/**PROC-**********************************************************************/
void shelf_store_orders(ORDER_LL_NODE **this_cycle_order) {
    ORDER_LL_NODE *iter, *prev = NULL, *ll_node_to_free;
    bool order_shelved_success = false;
    char time_str_buf[64];
    
    current_time_msec(time_str_buf);
    if(SYSTEM_DEBUG_LEVEL & L2) printf("%s: shelf   : L2: started shelving ingested orders this_cycle_order %p\n", 
                            time_str_buf, *this_cycle_order);  

    //For the first time, start from LL HEAD; for subsequent times, from last
    //run's TAIL's next item (which is the first items read in "this" ingestino
    //cycle
    iter = (g_data->g_order_ll_head==*this_cycle_order) ? 
                        *this_cycle_order : (*this_cycle_order)->next;
    
    while(iter) {
        ORDER *order = iter->data;
        if(SYSTEM_DEBUG_LEVEL & L2) printf("%s: shelf   : L2: order id %s temp %s\n", time_str_buf, order->id, 
                    ordertemp_to_str(order->temp));
        SHELF s = (SHELF)(order->temp);
        
        switch(order->temp) {
        case HOT:
        case COLD:
        case FROZEN:
            order_shelved_success = shelf_place_order_in_shelf(order, &s, 
                                                ordershelf_to_max_size(order->temp));
            break;
        default:
            break;
        }
        
        if(!order_shelved_success) {
            print_event_shelf_contents(ORDER_DISCARDED_SHELF_FULL);
            ll_node_to_free = iter; //to free this LL node
            if(prev) {
                prev->next = iter->next;
                iter = prev->next;
            } else {
                iter = iter->next;
            }   

            bool is_tail = g_data->g_order_ll_tail==ll_node_to_free;
            if(is_tail) {
                g_data->g_order_ll_tail = prev ? prev : *this_cycle_order;
                if(!prev) (*this_cycle_order)->next = NULL; //no items shelved in this cycle
            }
            
            bool is_head = g_data->g_order_ll_head==ll_node_to_free;
            if(is_head) {
                g_data->g_order_ll_head = iter;
                *this_cycle_order = iter; //let calling point know
            }
            
            if(SYSTEM_DEBUG_LEVEL & L3) printf("%s: shelf   : L3: is_tail ? %s is_head ? %s iter is NULL ? %s iter %p ll_node_to_free %p prev %p\n", 
                        time_str_buf, is_tail? "YES": "NO", 
                        is_head ? "YES":"NO", iter ? "NO" : "YES", iter, ll_node_to_free, prev);
            
            //free order memory
            free(ll_node_to_free);
            
            if(SYSTEM_DEBUG_LEVEL & L1) printf("%s: shelf   : L1: FREE order->id %p order->name %p order %p\n", 
                        time_str_buf, order->id, order->name, order);
            free(order->id);
            free(order->name);
            free(order);
            continue;
        } else {
            int *ptr_shelf = (int*)(malloc(sizeof(int)));
            *ptr_shelf = (int)s;
            if(SYSTEM_DEBUG_LEVEL & L1) printf("%s: shelf   : L1: order id %s shelf ptr %p\n", 
                        time_str_buf, order->id, ptr_shelf);
            g_hash_table_insert(g_data->g_order_id_shelf_hash, order->id, ptr_shelf);
        }
        
        prev = iter;
        iter = iter->next;
    }    
}

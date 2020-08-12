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

//Not a 'public' function; only internal to this file.
static char* ltrim(char* str) {
    if (!str)
        return '\0';
    if (!*str)
        return str;
    while (*str != '\0' && isspace(*str))
        str++;

    return str;
}

//Not a 'public' function; only internal to this file.
static char* rtrim(char* str) {
    if (!str)
        return '\0';
    if (!*str)
        return str;

    char* end = str + strlen(str) - 1;
    while (end >= str && isspace(*end)) {
        end--;
    }
    *(end + 1) = '\0';

    return str;
}

/**PROC+**********************************************************************/
/* Name:      read_properties                                                */
/*                                                                           */
/* Purpose:   To read "css.properties" file and load system properties       */
/*                                                                           */
/* Returns:   bool - for success/failure.                                     */
/*                                                                           */
/*                                                                           */
/* Operation: The function works as an "either-or". That is, if no           */
/* properties file is supplied, defaults are used (as per problem statement) */
/* On the other hand, if a file is specified, then it is expected that ALL   */
/* values are set in the file                                                */
/*                                                                           */
/**PROC-**********************************************************************/
bool read_properties() {
    bool success = true;
    char time_str_buf[64]; 
    char str[80]; //assumes rows in css.properties file are 80 column length
    char *trimmed_str, *key, *value;  
    
    FILE *f = fopen("css.properties" , "r");
    if(f == NULL) {
        current_time_msec(time_str_buf);        
        if(SYSTEM_DEBUG_LEVEL & L4) 
            printf("%s: input : L4: Cannot open css.properties file. Assuming defaults..\n", 
                        time_str_buf);
        HOT_SHELF_MAX_SIZE = DEFAULT_HOT_SHELF_MAX_SIZE;
        COLD_SHELF_MAX_SIZE = DEFAULT_COLD_SHELF_MAX_SIZE;
        FROZEN_SHELF_MAX_SIZE = DEFAULT_FROZEN_SHELF_MAX_SIZE;
        OVERFLOW_SHELF_MAX_SIZE = DEFAULT_OVERFLOW_SHELF_MAX_SIZE;

        KITCHEN_INGESTION_INTERVAL = DEFAULT_KITCHEN_INGESTION_INTERVAL;
        KITCHEN_INGESTION_RATE = DEFAULT_KITCHEN_INGESTION_RATE;
        KITCHEN_COURIER_DISPATCH_INTERVAL_MIN = DEFAULT_KITCHEN_COURIER_DISPATCH_INTERVAL_MIN; 
        KITCHEN_COURIER_DISPATCH_INTERVAL_MAX = DEFAULT_KITCHEN_COURIER_DISPATCH_INTERVAL_MAX;

        SHELF_MONITOR_INTERVAL = DEFAULT_SHELF_MONITOR_INTERVAL;
        SHELF_LIFE_MODIFIER_SINGLE_TEMP_SHELF = DEFAULT_SHELF_LIFE_MODIFIER_SINGLE_TEMP_SHELF;
        SHELF_LIFE_MODIFIER_OVERFLOW_SHELF = DEFAULT_SHELF_LIFE_MODIFIER_OVERFLOW_SHELF;
        
        SYSTEM_DEBUG_LEVEL = DEFAULT_DEBUG_LEVEL;
        SYSTEM_ORDERS_INPUT_FILE = malloc(strlen(DEFAULT_SYSTEM_ORDERS_INPUT_FILE)+1);
        strcpy(SYSTEM_ORDERS_INPUT_FILE, DEFAULT_SYSTEM_ORDERS_INPUT_FILE);
        
        SYSTEM_PRINT_SHELF_CONTENTS = DEFAULT_SYSTEM_PRINT_SHELF_CONTENTS;
    } else {
        bool is_eof = false;
        current_time_msec(time_str_buf);        
        while(!is_eof) {
            char *s = fgets(str, 80, f);
            if(!s) {
                is_eof = true;
                break;
            }
            trimmed_str = rtrim(ltrim(str));
            //printf("%s: input :L1: line %s\n", time_str_buf, trimmed_str);
            if(strlen(trimmed_str)==0 || trimmed_str[0] == '#' || trimmed_str[0] == '!')
                continue;
            key = rtrim(strtok(trimmed_str,"="));
            value = ltrim(strtok(NULL,"="));
            if(strcmp(key, "shelf.hot_shelf_max_size") == 0) {
                HOT_SHELF_MAX_SIZE = atoi(value);
            } else if(strcmp(key, "shelf.cold_shelf_max_size") == 0) {
                COLD_SHELF_MAX_SIZE = atoi(value);
            } else if(strcmp(key, "shelf.frozen_shelf_max_size") == 0) {
                FROZEN_SHELF_MAX_SIZE = atoi(value);
            } else if(strcmp(key, "shelf.overflow_shelf_max_size") == 0) {
                OVERFLOW_SHELF_MAX_SIZE = atoi(value);
            } //
            
              else if(strcmp(key, "kitchen.ingestion.interval") == 0) {
                KITCHEN_INGESTION_INTERVAL = atoi(value);
            } else if(strcmp(key, "kitchen.ingestion.rate") == 0) {
                KITCHEN_INGESTION_RATE = atoi(value);
            } else if(strcmp(key, "kitchen.courier.dispatch.interval.min") == 0) {
                KITCHEN_COURIER_DISPATCH_INTERVAL_MIN = atoi(value);
            } else if(strcmp(key, "kitchen.courier.dispatch.interval.max") == 0) {
                KITCHEN_COURIER_DISPATCH_INTERVAL_MAX = atoi(value);
            } 
            
              else if(strcmp(key, "shelf.monitor.interval") == 0) {
                SHELF_MONITOR_INTERVAL = atoi(value);
            } else if(strcmp(key, "shelflife.modifier.single.temp.shelf") == 0) {
                SHELF_LIFE_MODIFIER_SINGLE_TEMP_SHELF = atoi(value);
            } else if(strcmp(key, "shelflife.modifier.overflow.temp.shelf") == 0) {
                SHELF_LIFE_MODIFIER_OVERFLOW_SHELF = atoi(value);
            } else if (strcmp(key, "system.debug.level") == 0) {                
                SYSTEM_DEBUG_LEVEL = (strcmp(value,"NONE")==0) ? NONE : 
                                        ((strcmp(value,"L4")==0) ? L4 :
                                            ((strcmp(value,"L3")==0) ? (L4 | L3) :
                                                ((strcmp(value,"L2")==0) ? (L4 | L3 | L2) :
                                                    ((strcmp(value,"L1")==0) ? (L4 | L3 | L2 | L1) : NONE))));
            } else if (strcmp(key, "system.orders.file.name") == 0) {
                SYSTEM_ORDERS_INPUT_FILE = malloc(strlen(value)+1);
                strcpy(SYSTEM_ORDERS_INPUT_FILE, value);
            } else if(strcmp(key, "system.print.shelf.contents") == 0) {
                SYSTEM_PRINT_SHELF_CONTENTS = (strcmp(value,"true")==0) ? true : false;
            } else {
                //unknown property
                printf("%s: input :L1: unknown property key %s value %s\n", time_str_buf, key, value);
            }
        }
        fclose(f);
        
        //TODO: Add validation for values set via properties file; also to set default values
        //for properties that were NOT set via the properties file.
        //Set the return value based on the validation
    }
        
    return success;
}

/**PROC+**********************************************************************/
/* Name:      file_read_orders                                               */
/*                                                                           */
/* Purpose:   To read "orders.json" file and ingest orders                   */
/*                                                                           */
/* Params:    IN     f               - Pointer to orders input file          */
/*            IN     ingestion_rate  - Count of orders to be ingested in     */
/*                                                                           */
/* Returns:   bool - for EOF (true) or otherwise (false).                    */
/*                                                                           */
/*                                                                           */
/* Operation: The function works strictly uses the schema of sample file     */
/* "oders.json"; once read, it creates an ORDER instance in heap memory      */
/* and stores in a LL (linked list) to be consumed by the "kitchen" thread   */
/* "kitchen" is the caller.                                                  */
/* In theory                                                                 */
/*                                                                           */
/**PROC-**********************************************************************/
bool file_read_orders(FILE *f, int ingestion_rate) {
    bool is_eof = false;
    int read_count = 0, i;
    char str[64]; //TODO: Assuming 64 as the max length of char in orders file; revisit
    char *trimmed_str;
    ORDER *order;
    char time_str_buf[64];  
    
    current_time_msec(time_str_buf);
    while(read_count < ingestion_rate)
    {
        char *s = fgets(str, 64, f);
        if(!s) {
            is_eof = true;
            break;
        }
        
        trimmed_str = rtrim(ltrim(str));
        //printf("line %s\n", trimmed_str);
        if(strcmp(trimmed_str, "[") == 0) {
            continue;
        } else if (strcmp(trimmed_str, "]") == 0) {
            is_eof = true;
            break; //EOF
        } else if(strcmp(trimmed_str, "{") == 0) {
            //start of a record
            char *token, *token2, *token3;

            order = malloc(sizeof(ORDER));
            ftime(&order->creationTime);
            if(SYSTEM_DEBUG_LEVEL & L1) printf("%s: input   : L1: MALLOC order ptr %p\n", time_str_buf, order);
            for(i = 0; i < 5; i++) {
                fgets(str, 64, f);
                trimmed_str = rtrim(ltrim(str));
                token = strtok(trimmed_str, "\"");
                token2 = strtok(NULL, "\"");
                token3 = strtok(NULL, "\"");
                
                if(strcmp(token, "id") == 0) {
                    order->id = malloc(sizeof(char) * (strlen(token3)+1));
                    if(SYSTEM_DEBUG_LEVEL & L1) printf("%s: input   : L1: MALLOC order id ptr %p\n", time_str_buf, order->id);            
                    strcpy(order->id, token3);
                } else if(strcmp(token, "name") == 0) {
                    order->name = malloc(sizeof(char) * (strlen(token3)+1));
                    if(SYSTEM_DEBUG_LEVEL & L1) printf("%s: input   : L1: MALLOC order name ptr %p\n", time_str_buf, order->name);            
                    strcpy(order->name, token3);
                } else if (strcmp(token, "temp") == 0) {
                    if(strcmp(token3, "hot") == 0) {
                        order->temp = HOT;
                    } else if(strcmp(token3, "cold") == 0) {
                        order->temp = COLD;
                    } else if(strcmp(token3, "frozen") == 0) {
                        order->temp = FROZEN;
                    }
                } else if (strcmp(token, "shelfLife") == 0) {
                    token3 = strtok(token2, ",");
                    order->shelfLife = atoi(token2+2);
                } else if (strcmp(token, "decayRate") == 0) {
                    sscanf(token2+2, "%f", &(order->decayRate));
                }
            }
        } else if(trimmed_str[0] == '}') {
            //end of record
            //printf("End of record\n");
            read_count ++;
            ORDER_LL_NODE *node = malloc(sizeof(ORDER_LL_NODE));
            if(SYSTEM_DEBUG_LEVEL & L1) printf("%s: input   : L1: ORDER_LL_NODE ptr %p\n", time_str_buf, node);
            node->data = order;
            node->next = NULL;

            if(g_data->g_order_ll_head == NULL) {
                g_data->g_order_ll_head = node;
                g_data->g_order_ll_tail = node;
            } else {
                ORDER_LL_NODE *ll_iter = g_data->g_order_ll_head, *prev = NULL;
                while(ll_iter) {
                    prev = ll_iter;
                    ll_iter = ll_iter->next;
                }
                prev->next = node;
                g_data->g_order_ll_tail = node;
            }
        }       
    }
            
    return is_eof;
}

//TODO: unify all order free logic here
void free_order(ORDER **pOrder) {
    ORDER *order = (pOrder != NULL) ? *pOrder : NULL;
    if(order) {
        free(order->id);
        free(order->name);
        
        free(order);
        *pOrder = NULL;
    }   
}


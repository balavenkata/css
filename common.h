#ifndef COMMON_H
#define COMMON_H

//ENUMs
//Temperature enum
typedef enum temp_t {
    HOT = 0,
    COLD = 1,
    FROZEN = 2,
    MAX_TEMP = 3
} TEMP;

//Shelf enum
typedef enum shelf_t {
    HOT_SHELF = 0,
    COLD_SHELF = 1,
    FROZEN_SHELF = 2,
    OVERFLOW_SHELF = 3,
    MAX_SHELF = 4
} SHELF;

//Event enum
typedef enum order_event_t {
    ORDER_READ = 0,
    ORDER_DELIVERED = 1,
    ORDER_DISCARDED_SHELF_FULL = 2,
    ORDER_DISCARDED_STALE = 3,
    MAX_EVENT = 4
} ORDER_EVENT;

typedef enum debug_level_t {
    NONE = 0,
    L1 = 1,
    L2 = 2,
    L3 = 4,
    L4 = 8
} DEBUG_LEVEL;

//STRUCTs
typedef struct order_t {
    char *id;
    char *name;
    TEMP temp;
    int shelfLife;
    float decayRate;
    struct timeb creationTime;
} ORDER;

typedef struct order_ll_node_t {
    ORDER *data;
    struct order_ll_node_t *next;
} ORDER_LL_NODE;

typedef struct data_t {
    ORDER_LL_NODE *g_order_ll_head;
    ORDER_LL_NODE *g_order_ll_tail;

    //<order_id> - <SHELF>
    GHashTable *g_order_id_shelf_hash;
    //HOT_SHELF <order_id> - <Order>
    GHashTable *g_order_id_hot_shelf_hash;
    //COLD_SHELF <order_id> - <Order>
    GHashTable *g_order_id_cold_shelf_hash;
    //OVERFLOW_SHELF <order_id> - <Order>
    GHashTable *g_order_id_frozen_shelf_hash;
    //FROZEN_SHELF <order_id> - <Order>
    GHashTable *g_order_id_overflow_shelf_hash;
    
    //OVERFLOW_SHELF contents...grouped by temperature...2-d array of ORDER*
    //..{TEMP][OVERFLOW_SHELF_MAX_SIZE]
    ORDER ***g_overflow_by_temp_array;
    int *g_overflow_by_temp_array_sz;
} DATA;

//GLOBALs
pthread_t kitchen_thread_id;
pthread_t courier_thread_id;
pthread_t monitor_thread_id;

pthread_mutex_t data_access_mutex;
pthread_cond_t orders_empty_cond;

DATA *g_data;

//Common functions
GHashTable *shelf_to_hash(SHELF shelf);
void *monitor_thread_cb();
void current_time_msec(char *buf);

#endif
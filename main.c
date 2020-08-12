#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <sys/timeb.h>

#include "common.h"
#include "kitchen.h"
#include "courier.h"

void main()
{
    if(init()) {    
        //Three "worker" threads doing 3 different jobs
        //  1. Kitchen thread - reads input (models "taking" an order), then 
        //                      schedules pickup at a random time (in a range)
        //  2. Courier thread - this is a "timer" thread which is used to
        //                      model the act of a courier coming at the random
        //                      time set by the 'kitchen' and picking up for delivery
        //  3. Monitor thread - this models the periodic inspection of the shelf 
        //                      for stale orders. If stale, this thread removes 
        //                      those orders
        pthread_create(&kitchen_thread_id, NULL, kitchen_thread_cb, NULL);
        pthread_create(&courier_thread_id, NULL, courier_timer_thread_cb, NULL);
        pthread_create(&monitor_thread_id, NULL, monitor_thread_cb, NULL);
        
        //If kitchen is done, it is time to stop the system
        pthread_join(kitchen_thread_id, NULL); //kitchen_thread cancles courier upon file read finish  
        
        finalize();
    } else {
        printf("!!! SYSTEM INIT FAILED !! ABORTING\n");
    }
}

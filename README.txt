README
=======

OVERVIEW
--------
The real-time system created (call it "css" which is the program name)
implements the requirements specified in the instructions shared.

DESIGN
------
Please refer to the DESIGN_FLOW.txt file associated for details
discussed below. This DESIGN section talks about the threads used
in css & reasons for their choices. It also discusses the data structures
used in css.

At a high level these are the coordinating threads:
1. main thread
2. kitchen thread
3. courier thread
4. monitor thread

main thread & data structure design
************************************
The main thread starts of the day with initializing global data by reading
css.properties, if available. NOTE: here, the global data contains mostly
in-memory structures such as Hashtables and arrays.

    1. There is one overall hash for <order id> - <shelf>
       The use of this overall hash is for quick pickup by courier
    2. There are 4 hashtables one for each of {hot, cold, frozen, overflow}
       shelves. They stores per shelf orders such as <order id> - <pointer to
       order>
    3. There is an additional data structure for overflow shelf which is
       a 2-d array of just the overflow shelf contents.
       Its dimentions are [temperature][overflow_shelf_sz].
       The use of this is during handling the case when overflow shelf is full.
       When full, we need to first move an item to its temperature matching
       shelf.To do this lookup fast, the logic finds first which of {hot, cold,
       frozen} shelves have space, and if there is an orderof matching
       temperature in overflow shelf, it moves it to its temperature shelf.
       NOTE: the idea of this 2-d array is also to randomly discard an
       existing order if a move from overflow shelf is not possible. A random
       index can be used to pick an order from overflow shelf for discard.
       However, currently the css DOES NOT do that; instead it discards the
       order that is being tried to shelf.
    4. Lastly there is a LL (Linked List) which tracks the orders read in a
       linear fashion for fast iteration. Upon reading from input the LL is
       built. It is then quickly iterated for shelving. At any time the LL
       contains one of these
            i. Both head and tail being NULL (on initialization of read failure)
            ii. Head and tail nodes set just after file read
            iii. After shelving the head and tail nodes are adjusted if some
                 orders get fail to be shelved.
    5. To protect data being corrupted by concurrent threads a mutex is used
       while accessing the hashtables & 2-d array.
    6. A condition (signal) variable is used to coordinate between kitchen
       and courier threads.

kitchen thread
**************
Kitchen thread does following tasks:
    1. Reads order from system and immediately "cooks" it
    2. Then it schedules the order for pickup. Here, the "schedules" happens by
       creating a timer entry whose callback happens in courier thread.
    3. Both the ingestion parameters (how many orders to read from file as well
       as how often to read are configurable). Please refer to "css.properties"
       file comments.
    4. When all entries have been read, it "waits" on a condition variable. This
       simulates the waiting for all orders to be picked up for delivery by the
       courier.
    5. When courier thread signals (using a condition variable), kitchen thread
       does the final cleanup.
    6. It also takes care of canceling the other two threads.

courier thread
**************
Courier thread does the following tasks
    1. On start of the day it "waits" for a timer task (models an order to be
       scheduled for delivery by the kitchen)
    2. Once timer nodes are created, the corresponding fd's (file descriptors)
       are polled for activity.
    3. On timer expiry the poll event happens and the callback is called (the
       callback simlates a pickup and instant delivery)
    4. Once an order is delivered it is purged from the system

monitor thread
**************
Monitor thread does the job of monitoring the shelf for order staleness. It
runs in configurable periodicity. When the time tick happens (that is, when
it has to monitor), it goes thru all shelf hash entries, calculates the
"value" for orders. It then uses the value to see if an order is stale. If
stale, it purges that order from the system.


INSTRUCTIONS TO RUN
---------------------
1. The source code consists of 7 C source files & 4 header files
    -rwxr--r-- 1 bvenkata sw-team  1236 Jul 18 00:58 main.c
    -rwxr--r-- 1 bvenkata sw-team 13627 Jul 18 02:02 courier.c
    -rwxr--r-- 1 bvenkata sw-team  5221 Jul 18 02:03 monitor.c
    -rwxr--r-- 1 bvenkata sw-team  9988 Jul 18 02:04 shelf.c
    -rwxr--r-- 1 bvenkata sw-team 11642 Jul 18 03:06 input.c
    -rwxr--r-- 1 bvenkata sw-team 11743 Jul 18 03:10 utils.c
    -rwxr--r-- 1 bvenkata sw-team  6564 Jul 18 03:13 kitchen.c

    -rwxr--r-- 1 bvenkata sw-team  572 Jul 17 06:27 courier.h
    -rwxr--r-- 1 bvenkata sw-team  162 Jul 17 06:54 kitchen.h
    -rwxr--r-- 1 bvenkata sw-team 1950 Jul 18 02:00 common.h
    -rwxr--r-- 1 bvenkata sw-team 1399 Jul 18 02:58 constants.h

2. There is a make file ("Makefile") that can be used on a system that
can do compile/build using GNU make or similar tools.

3. There is a "css.properties" file that can be used to tweak various
parameters of the css system. Comments in the file should help explain
them.

4. All test code are located in sub-directory "test"

4. To run, the binary can be involed directly without any parameters.
On a Linux system it starts off as follows; it prints shelf contents
on key events always. Other debugs can be controller (see css.properties
file to know how to control debug output)

    Sat Jul 18 04:05:25 ::css?./css
2020-07-18 04:08:29.967: shelf   : L2: started shelving ingested orders
2020-07-18 04:08:29.967: shelf   : L2: order id a8cfcb76-7f24-4420-a5ba-d46dd77bdffd temp FROZEN
2020-07-18 04:08:29.967: shelf   : L2: order id 4f304b59-6634-4558-a128-a8ce12b1f818 temp FROZEN
-------------------------------
EVENT: ORDER_READ
SHELF: [HOT_SHELF]
CONTENTS:[]
SHELF: [COLD_SHELF]
CONTENTS:[]



INSTRUCTIONS TO BUILD
---------------------
1. A simple "make" on Linux builds the system. The binary "css" is the output
   which is to be run.
2. The build uses glib-2.0 which is needed in the target build system.
3. The unit tests are written using CUnit framework - to compile them please
   download from http://cunit.sourceforge.net/
4. There are some warnings reported from glib files which can be ignored.
5. The system I used was this:

Sat Jul 18 05:34:11 ::css?uname -a
Linux bvenkata-vm 2.6.32-279.22.1.el6.x86_64 #1 SMP Sun Jan 13 09:21:40 EST 2013 x86_64 x86_64 x86_64 GNU/Linux
Sat Jul 18 05:34:13 ::css?


KNOWN ISSUES (& FIX IDEAS) AND IMPROVEMENTS (& HOW TO IMPLEMENT THEM)
----------------------------------------------------------------------
CAVEATS / ISSUES
*****************
1. Currently the max number of timers (for "courier") is set to 1000.
(if you search in code for "MAX_TIMER_COUNT" this will show up). On the system
the test was run, higher values were throwing random errors. This could very
well scale up on a better system

2. The "css.properties" read by the css system are not verified for validity.
For instance, to message the courier for pickup a randomized value is used.
Per problem statement, this is 2-6 seconds. The properties file has a way to
specify this. However it does not ensure the min/max values are set right
(such as min < max). Adding this validity for all property values should be
relatively trivial.

3. Per problem statement, when shelving an order, if the overflow shelf is
full and also no existing orders in overflow shelf can be moved to its
corresponding temperature shelf, an "existing order" should be discarded.
However the css system discards the new order to be shelved. A better logic
could be to choose an order that is close to expiry (using its "value" which
decreases over age). This was the idea I had but due to lack of time, could
not implement.

IMPROVEMENTS
*************
1. Though monitor thread periodically runs and purges stale orders, there is a
small window where, after the monitor thread runs and before the pickup happens,
the order can go stale. An improvement could be made to also check the order
for its "value" (i.e. age and staleness) so that a stale order can be
discarded instead of picked up & delivered.

2. I ran valgrind memory check tool which was reporting some unreleased memory
by glib whose GHashtable implementation I used in the css system. The leaked
memory wasn't substantial using sample orders file (18K). This needs further
investigation (a possible bug in glib GHashtable).

==11692== 16,384 bytes in 1 blocks are still reachable in loss record 6 of 6
==11692==    at 0x4C2CECB: malloc (in /usr/lib/valgrind/vgpreload_memcheck-amd64-linux.so)
==11692==    by 0x50A5BB8: g_malloc (in /usr/lib/libglib-2.0.so.0.5400.3)
==11692==    by 0x50AFF3B: ??? (in /usr/lib/libglib-2.0.so.0.5400.3)
==11692==    by 0x400EE89: call_init.part.0 (dl-init.c:72)
==11692==    by 0x400EF92: call_init (dl-init.c:30)
==11692==    by 0x400EF92: _dl_init (dl-init.c:119)
==11692==    by 0x4001069: ??? (in /lib/ld-2.27.so)

3. File read method can be replaced with DB (database) read/query for
ingesting orders. Currently the file pointer is used as cursor to
continue on each kitchen cycle/run. For DB, either a cursor (on DB systems
supporting that) or a mechanism to store the last ingested point is needed

4. Currently the css system keeps all data in memory. This method is
susceptible to failure (if the host running css fails, disk/memory failure
happens etc.). It would be prudent to have a mirror system for high
availability - one simple approach is to use the file system itself. But
for production grade, one needs a more robust HA framework + associated
infra such as messaging to the secondary store etc.

5. An item I had in mind was also implementing the following
    a. Statistics for {ingested, discarded, delivered} orders. This can
    be a simple global structure with integer counters for the three
    events that get incremented. It can either be reported at the end of
    the run OR periodically (say during monitor run)

    b. Another idea was to have a simple socket listener whose job is to
    respond to queries from a client to report on the aforementioned
    statistics. One can use a key-value schema for the query request and
    response.

6. Perhaps have two pthread's doing the "kitchen worker thread" job such
that we can interleave them. File I/O read function be optimized a bit
to skip the mutex lock/unlock so that two "kitchen threads" can ingest.


LICENSES
--------
1. I don't know of any license requirements from glib. On the Linux system I
used it was already available.

2. CUnit is an open source framework with a free to use license.

3. I am not aware of any other license in the css system. If so, the respective
authors have to be contacted.


- Bala Venkata (Author)
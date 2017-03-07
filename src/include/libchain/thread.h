/** @file thread.h
 *  @brief Interface for multi-threaded chain functions
 *
 */

#ifndef _THREAD_H
#define _THREAD_H

#include "chain.h"

#define MAX_NUM_THREADS 4


typedef struct thread_t {
    // TODO - overflow is possible!
    unsigned thread_id;
    context_t context;
} thread_t;

typedef struct thread_state_t {
    thread_t thread;
    unsigned active;
} thread_state_t;

struct thread_array {
    SELF_CHAN_FIELD_ARRAY(thread_state_t, threads, MAX_NUM_THREADS);
    SELF_CHAN_FIELD(unsigned, current);
    SELF_CHAN_FIELD(unsigned, num_threads);
};

__nv extern thread_t * volatile cur_thread; 

extern SELF_CHANNEL_DEC(scheduler_task,thread_array); 

/** @brief Initialize scheduler constructs at first boot */
void thread_init();

/** @brief Terminate execution of a thread, removing it from
 *         the scheduling pool
 *  @return Void
 */
void thread_end();

/** @brief Creates a separate thread to run task new_task
 *  @param new_task Task entry point for the thread
 *  @return 0 on success, a negative error code on failure
 */
int thread_create(task_t *new_task);

/** @brief Gets the currently running thread
 *  @return Pointer to the struct describing the current thread
 */
thread_t *get_current_thread();

/** @brief returns a pointer to the current thread */ 
uint8_t getThreadPtr(); 


#endif

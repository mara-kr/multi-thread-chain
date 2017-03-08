/** @file thread.h
 *  @brief Interface for multi-threaded chain functions
 *
 */

#ifndef _THREAD_H
#define _THREAD_H

#include "chain.h"

typedef enum sched_fields_{ 
	threads,
	thread,
	current, 
	num_threads
}sch_chan_fields ; 

typedef struct thread_t {
    // TODO - overflow is possible!
    unsigned thread_id;
    context_t context;
} thread_t;

__nv extern thread_t * volatile cur_thread; 

//extern SELF_CHANNEL_DEC(scheduler_task,thread_array); 
//extern struct _ch_type_scheduler_task_scheduler_task_thread_array
//							_ch_scheduler_task_scheduler_task; 

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

void write_to_scheduler(sch_chan_fields field, void *input);

void read_from_scheduler(sch_chan_fields field, void *output); 
#endif

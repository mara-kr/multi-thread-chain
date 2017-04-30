/** @file thread.h
 *  @brief Interface for multi-threaded chain functions
 *
 */

#ifndef _THREAD_H
#define _THREAD_H

#include "chain.h"

#define MAX_NUM_THREADS 4

#define TRANSITION_TO_MT(task) transition_to_mt(TASK_REF(task))

typedef struct thread_t {
    // TODO - overflow is possible!
    unsigned thread_id;
    context_t context;
} thread_t;

__nv extern thread_t * volatile cur_thread;

//extern SELF_CHANNEL_DEC(scheduler_task,thread_array);
//extern struct _ch_type_scheduler_task_scheduler_task_thread_array
//							_ch_scheduler_task_scheduler_task;

#define THREAD_CREATE(task) thread_create(TASK_REF(task))

//For consistency in macro-omnipresence
#define THREAD_END() thread_end()
/** @brief Initialize the multi-threading library at first boot */
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
thread_t get_current_thread();

/** @brief Deschedules the running thread
 *  @return Void
 */
void deschedule();

void transition_to_mt(task_t *next_task);

/** @brief returns a pointer to the current thread */
uint8_t getThreadPtr();

/** @brief Enable interrupts on the msp430 */
void enable_interrupts();

void _interrupt_setup();
void return_from_interrupt();


/** @brief Designate function to run on interrupt firing on Port 1 */
#define INTERRUPT_TASK(func) \
    _interrupt void Port_1() { \
        _interrupt_setup(); \
        func();\
    }

#define IRET return_from_interrupt();

#endif

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
    // TODO - overflow is possible (but chain has a similar issue)
    unsigned thread_id;
    context_t context;
} thread_t;


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


/******************************************
 * Interrupt handling functions & defs
 * ****************************************/

/** @brief Enable interrupts if we aren't in an interrupt handler
 *         otherwise, do nothing. */
void enable_interrupts()

void _interrupt_prologue(void *func);

/** @brief Whether execution is inside an interrupt handler */
int in_interrupt_handler();

#define INT_SETUP_COMPLETE() \
    _int_setup_complete = 1; \
    _enable_interrupt();

/** Designate function to run on interrupt firing on Timer0_AO */
/** @brief Macro for user to define task to run upon an interrupt firing.
 *
 *  _interrupt_prologue(func) is the precursor to receiving an interrupt.
 *  If a power failure occurs between recieving the interrupt and
 *  writing curctx->task in _interrupt_prologue, the interrupt will not
 *  be recieved.
 */
#define INTERRUPT_TASK(val, func) \
__attribute__((interrupt(TIMER0_A0_VECTOR))) void Timer0_A0_ISR (void) { \
        _interrupt_prologue(func); \
        func(); \
    } \
    TASK(val, func)
void return_from_interrupt();
#define IRET() return_from_interrupt();

#endif

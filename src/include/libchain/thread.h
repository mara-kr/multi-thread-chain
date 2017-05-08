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

extern unsigned curr_free_index;
extern uint8_t _int_reboot_occurred;

//#define __enable_interrupt() __bis_SR_register(GIE)

// Lowest bit of task->func is set if task is inside an interrupt handler
#define TASK_FUNC_INT_FLAG 0x0001U

#define CLEAR_INT_FLAG(func) ((void *) \
        (((unsigned) (func)) & ~(TASK_FUNC_INT_FLAG)))
#define GET_INT_FLAG(func) (((unsigned) (func)) & (TASK_FUNC_INT_FLAG))
#define SET_INT_FLAG(func) ((void *) \
        (((unsigned) (func)) | (TASK_FUNC_INT_FLAG)))


/** @brief Enable interrupts if we aren't in an interrupt handler
 *         otherwise, do nothing. */
void enable_interrupts();

void _interrupt_prologue(task_t *int_task);

/** @brief Whether execution is inside an interrupt handler */
int in_interrupt_handler();

/** @brief Call when user-mode setups for interrupts is complete */
#define INT_SETUP_COMPLETE() int_setup_complete()
void int_setup_complete();

/** Designate function to run on interrupt firing on Timer0_AO */
/** @brief Macro for user to define task to run upon an interrupt firing.
 *
 *  _interrupt_prologue(func) is the precursor to receiving an interrupt.
 *  If a power failure occurs between recieving the interrupt and
 *  writing curctx->task in _interrupt_prologue, the interrupt will not
 *  be recieved.
 */
#define INTERRUPT_TASK(val, func) \
    TASK(val, func) \
    __attribute__((interrupt(TIMER0_A0_VECTOR))) void Timer0_A0_ISR (void) { \
        _interrupt_prologue(TASK_REF(func)); \
        func(); \
    }

void return_from_interrupt();
#define IRET() return_from_interrupt();

#endif

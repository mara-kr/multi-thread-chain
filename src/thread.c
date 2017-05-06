/** @file thread.c
 *  @brief Implementation of multi-threading functions
 */

#include <stdarg.h>
#include <string.h>

#ifndef LIBCHAIN_ENABLE_DIAGNOSTICS
#define LIBCHAIN_PRINTF(...)
#else
#include <stdio.h>
#define LIBCHAIN_PRINTF printf
#endif

#include "chain.h"
#include "thread.h"


// Current index in the free indicies array
unsigned curr_free_index;

typedef struct thread_state_t {
    thread_t thread;
    unsigned active;
} thread_state_t;

struct thread_lib_fields {
    // The current running thread
    SELF_CHAN_FIELD(unsigned, current);
};

#define FIELD_INIT_thread_lib_fields {\
    SELF_FIELD_INITIALIZER,\
}

struct thread_array {
    // Array of thread information
    CHAN_FIELD_ARRAY(thread_state_t, threads, MAX_NUM_THREADS);
};

struct indicies {
    // Indexes of spots in the thread array that aren't in use
    CHAN_FIELD_ARRAY(unsigned, free_indicies, MAX_NUM_THREADS);
    // Size of the above array
    CHAN_FIELD(unsigned, size);
};

unsigned get_current();
static void set_current(unsigned current);


// TODO - we should use a different #define so that the
// scheduler task/channel symbols don't conflict and are easy to
// access
TASK(5, scheduler_task)

// Broken self-channel for current
SELF_CHANNEL(scheduler_task, thread_lib_fields);
#define THREAD_FIELDS_CH (SELF_CH(scheduler_task))
// Broken channel - all threads write, scheduler_task reads
CHANNEL(task_global, scheduler_task, thread_array);
#define THREAD_ARRAY_CH (CH(task_global, scheduler_task))
// Broken channel - scheduler task defines free slots in threads[]
CHANNEL(scheduler_task, task_global, indicies);
#define INDICIES_CH (CH(scheduler_task, task_global))


// Task to represent all tasks for "broken channels" - channels that any task
// can read XOR write to
void task_global() {
    return;
}


// Do all the things to run the scheduler
void scheduler_task() {
    task_prologue(); // Swap buffers for scheduler self_channel
    LIBCHAIN_PRINTF("Inside scheduler task!! \r\n");

    // There's probably a better way to get an array into volatile memory
    thread_state_t threads[MAX_NUM_THREADS];
    unsigned indicies_size = 0;
    // Last entry used for interrupt thread
    for (unsigned i = 0; i < MAX_NUM_THREADS - 1; i++) {
        threads[i] = *CHAN_IN1(thread_state_t, threads[i], THREAD_ARRAY_CH);
        // Set the indicies array to avoid two traversals
        if (!threads[i].active) {
            CHAN_OUT1(unsigned, free_indicies[indicies_size], i, INDICIES_CH);
            indicies_size++;
        }
    }

    // Write the size of the indicies array
    CHAN_OUT1(unsigned, size, indicies_size, INDICIES_CH);
    // Zero the current free index since we just wrote the scheduler array
    curr_free_index = 0;

    unsigned current = get_current();

    // Round robin - start with the next potentially schedulable thread
    unsigned curr_idx = (current + 1) % MAX_NUM_THREADS;
    LIBCHAIN_PRINTF("curr idx = %u \r\n",curr_idx);
    // Look for the next task to schedule
    while (1) {
        LIBCHAIN_PRINTF("threads[curr_idx]=%i active=%u \r\n",curr_idx,
                        threads[curr_idx].active);
        if (threads[curr_idx].active) {
            current = curr_idx;
            break;
        }
        curr_idx = (curr_idx + 1) % MAX_NUM_THREADS;
    }

    set_current(current);
    thread_state_t curr_thread = threads[current];

    task_t *next_task = curr_thread.thread.context.task;

    transition_to(next_task);
}


// Transition to the next task in the current thread
void transition_to_mt(task_t *next_task){
    LIBCHAIN_PRINTF("transition_to_mt \r\n");
    thread_t next_thr;
    thread_state_t next_thr_state;
    context_t next_ctx;
    unsigned current = get_current();
    LIBCHAIN_PRINTF("Current = %u \r\n", current);

    next_ctx.task = next_task;
    next_ctx.time = curctx->time + 1;
    LIBCHAIN_PRINTF("transition_to_mt next task = %x \r\n",next_ctx.task);
    //Make thread_t to pass to scheduler
    thread_state_t curr_thr = *CHAN_IN1(thread_state_t, threads[current],
            THREAD_ARRAY_CH);
    next_thr.thread_id = curr_thr.thread.thread_id;
    next_thr.context = next_ctx;

    //Write thread out to scheduler
    next_thr_state.thread = next_thr;
    next_thr_state.active = 1; // Hasn't called thread_end, still active

    //Update context passed in
    CHAN_OUT1(thread_state_t, threads[current], next_thr_state,
            THREAD_ARRAY_CH);
    TRANSITION_TO(scheduler_task);
}


void swap_scheduler_buffer(void){
  //So we don't have to keep calling TASK_REF...
  task_t *curtask = TASK_REF(scheduler_task);
  // Minimize FRAM reads
  self_field_meta_t **dirty_self_fields = curtask->dirty_self_fields;

  int i;

  // It is safe to repeat the loop for the same element, because the swap
  // operation clears the dirty bit. We only need to be a little bit careful
  // to decrement the count strictly after the swap.
  while ((i = curtask->num_dirty_self_fields) > 0) {
      self_field_meta_t *self_field = dirty_self_fields[--i];

      if (self_field->idx_pair & SELF_CHAN_IDX_BIT_DIRTY_CURRENT) {
          // Atomically: swap AND clear the dirty bit (by "moving" it over to MSB)
          __asm__ volatile (
              "SWPB %[idx_pair]\n"
              : [idx_pair]  "=m" (self_field->idx_pair)
          );
      }

      // Trade-off: either we do one FRAM write after each element, or
      // we do only one write at the end (set to 0) but also not make
      // forward progress if we reboot in the middle of this loop.
      // We opt for making progress.
      curtask->num_dirty_self_fields = i;
  }
}


/** @brief Setup the 0th index in the threads array to the current
 *         running thread, zero other elements of thread array,
 *         set current, initialize free indicies array & size
 */
void thread_init() {
    thread_state_t thread;
    thread.thread.context = *curctx;
    thread.thread.thread_id = 0;
    thread.active = 1;
    CHAN_OUT1(thread_state_t, threads[0], thread, THREAD_ARRAY_CH);

    // Zero out non-volatile active bits (except current running thread)
    for (unsigned i = 1; i < MAX_NUM_THREADS; i++) {
        thread = *CHAN_IN1(thread_state_t, threads[i],
                THREAD_ARRAY_CH);
        thread.active = 0;
        CHAN_OUT1(thread_state_t, threads[i], thread,
                THREAD_ARRAY_CH);
    }

    // Setup free indicies array - all free except index 0
    for (unsigned i = 1; i < MAX_NUM_THREADS; i++) {
        CHAN_OUT1(unsigned, free_indicies[i-1], i, INDICIES_CH);
        LIBCHAIN_PRINTF("Free= %i \r\n",i);
    }

    // Set the size of the free indicies array
    unsigned indicies_size = MAX_NUM_THREADS - 1;
    CHAN_OUT1(unsigned, size, indicies_size, INDICIES_CH);
    //Set the current thread to index 0
    set_current(0);
    swap_scheduler_buffer();
}


void thread_end() {
    unsigned current = get_current();
    LIBCHAIN_PRINTF("Ended thread %u \r\n", current);
    thread_state_t curr_thread = *CHAN_IN1(thread_state_t , threads[current],
        THREAD_ARRAY_CH);
    curr_thread.active = 0;
    CHAN_OUT1(thread_state_t, threads[current], curr_thread,
        THREAD_ARRAY_CH);
    TRANSITION_TO(scheduler_task);
}


int thread_create(task_t *new_task) {
    unsigned indicies_size = *CHAN_IN1(unsigned, size, INDICIES_CH);
    thread_state_t new_thread;
    LIBCHAIN_PRINTF("Inside thread create!! new task = %x\r\n", new_task);
    unsigned new_thr_slot = *CHAN_IN1(unsigned, free_indicies[curr_free_index],
                                      INDICIES_CH);
    LIBCHAIN_PRINTF("new_thr_slot = %u , curr_free= %u\r\n",
            new_thr_slot, curr_free_index);
    if (curr_free_index < indicies_size) {
        new_thread.thread.context.task = new_task;
        // TODO Set to creation time instead of 0?
        new_thread.thread.context.time = 0;
        new_thread.thread.context.next_ctx = NULL;

        CHAN_OUT1(thread_state_t, threads[new_thr_slot], new_thread,
            THREAD_ARRAY_CH);
        curr_free_index++;
        return 0;
    }
    return -1;
}


void deschedule() {
    task_t *curr_task = curctx->task;
    transition_to_mt(curr_task);
}


/***********************************************************
 * Interrupt handling code
 * *********************************************************/

void enable_interrupts() {
    unsigned curr_task_addr = (unsigned) curctx->task->func;
    if (!(curr_task_addr & TASK_FUNC_INT_FLAG)) {
        __enable_interrupt();
    }
}


/***********************************************************
 * Interrupt handling functions and variables
 * NOTE: Assumes that device resets to interrupts disabled
 ***********************************************************/

/** Whether an interrupt routine is running - if unset, _init should
 *  enable interrupts?
 */
__nv uint8_t interrupt_active = 0;

void enable_interrupts() {
    // TODO enable interrupt for specific device & clear int_flag
    //P1IE |= BIT3; // P1.3 interrupt enabled
    //P1IFG &= ~BIT3; // P1.3 interrupt flag cleared
    __bis_SR_register(GIE);
}


// Should be user-defined (TODO, better way?)
void interrupt_task();
TASK(interrupt_task, 7)

/** @brief Make it so that reboots restart the interrupt task
 *
 *  Runs before user defined interrupt handler
 *  NOTE: Stack decrement isn't idempotent - should only be
 *        run if no reboot has occured
 */
// TODO If power fails during interrupt, stack clears, need to
//      only run function once
// TODO Need to store some bit that says to go to the ISR in _init
//      set in _interrupt_setup(), cleared in ISR
//
//      Ideally, on reboot, skip _interrup_setup and go to user handler
//      Interrupt setup deals with the case where a restart doesn't occur
void _interrupt_setup() {
    // TODO Both operations need to happen at same time:
    //      if interrupt context && ~interrupt_active, ints could be reentrant
    //      if !int_context && int_active, interrupt would never fire again
    // FIX: Interrupts disabled by default on MSP430, still an issue!!!!

    // Set current context to user ISR (if possible)
    curctx->task = TASK_REF(interrupt_task);
    // Set _init to keep interrupts disabled
    interrupt_active = 1;
    // Clear stack of PC and SR
    __asm__ volatile ( // volatile because output operands unused by C
        "adda #4h, sp\n"
        : /* no outputs */
        : /* no inputs */
        : "sp"
        : /* no goto labels */
    );
    /* Clear the interrupt stack - if a power failure
     * occurs, the stack will be cleared - decrementing
     * after power failure could cause an error.
     * We decrement at the start and set a flag if the
     * operation has completed. */
}


void return_from_interrupt() {
    __bis_SR_register(GIE); // re-enable interrupts
    transition_to_mt(scheduler_task); // TODO - transition to old current?
}

/***********************************************************
 * Interface to write to the scheduler's broken self channel
 ***********************************************************/

static void scheduler_chan_out(const char *field_name, const void *value,
        size_t var_size, uint8_t *chan, size_t field_offset);


#define SCHEDULER_CHAN_OUT(type, field, val, chan0) \
    scheduler_chan_out(#field, &val, sizeof(VAR_TYPE(type)), \
             (uint8_t *) chan0, offsetof(__typeof__(chan0->data), field))


#define SCHEDULER_CHAN_IN(type, field, chan0) \
    CHAN_IN1(type, field, chan0)


/** @brief Get the index of the current running thread in threads[]
 */
unsigned get_current() {
    return *SCHEDULER_CHAN_IN(unsigned, current, THREAD_FIELDS_CH);
}


/** @brief Set the index of the current running thread in threads[]
 */
static void set_current(unsigned current) {
    SCHEDULER_CHAN_OUT(unsigned, current, current, THREAD_FIELDS_CH);
}


// Same as CHAN_OUT, but increments the number of dirty fields for
// the scheduler
static void scheduler_chan_out(const char *field_name, const void *value,
    size_t var_size, uint8_t *chan, size_t field_offset) {
    var_meta_t *var;

    uint8_t *chan_data = chan + offsetof(CH_TYPE(_sa, _da, _void_type_t), data);
    uint8_t *field = chan_data + field_offset;

    self_field_meta_t *self_field = (self_field_meta_t *)field;
    task_t *curtask = TASK_REF(scheduler_task);

    unsigned var_offset =
        (self_field->idx_pair & SELF_CHAN_IDX_BIT_NEXT) ? var_size : 0;

    var = (var_meta_t *)(field +
            offsetof(SELF_FIELD_TYPE(void_type_t), var) + var_offset);

    self_field->idx_pair &= ~(SELF_CHAN_IDX_BIT_DIRTY_NEXT);
    self_field->idx_pair |= SELF_CHAN_IDX_BIT_DIRTY_CURRENT;
    curtask->dirty_self_fields[curtask->num_dirty_self_fields++] = self_field;

    var->timestamp = curctx->time;
    void *var_value = (uint8_t *)var + offsetof(VAR_TYPE(void_type_t), value);
    memcpy(var_value, value, var_size - sizeof(var_meta_t));
}

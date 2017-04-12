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

#define MAX_NUM_THREADS 4

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

struct free_indicies {
    // Indexes of spots in the thread array that aren't in use
    CHAN_FIELD_ARRAY(unsigned, free_indicies, MAX_NUM_THREADS);
    // Size of the above array
    CHAN_FIELD(unsigned, size);
};

static unsigned get_current();
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
CHANNEL(scheduler_task, task_global, free_indicies);
#define INDICIES_CH (CH(scheduler_task, task_global))


// Task to represent all tasks for "broken channels" - channels that any task
// can read XOR write to
void task_global() {
    return;
}


// Do all the things to run the scheduler
void scheduler_task() {
    task_prologue(); // Swap buffers for scheduler self_channel

    // There's probably a better way to get an array into volatile memory
    thread_state_t threads[MAX_NUM_THREADS];
    unsigned indicies_size = 0;
    for (unsigned i = 0; i < MAX_NUM_THREADS; i++) {
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
    // Look for the next task to schedule
    while (1) {
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
    }
    // Set the size of the free indicies array
    unsigned indicies_size = MAX_NUM_THREADS - 1;
    CHAN_OUT1(unsigned, size, indicies_size, INDICIES_CH);

    //Set the current thread to index 0
    set_current(0);
}


void thread_end() {
    unsigned current = get_current();
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

    if (curr_free_index < indicies_size) {
        new_thread.thread.context.task = new_task;
        // TODO Set to creation time instead of 0?
        new_thread.thread.context.time = 0;
        new_thread.thread.context.next_ctx = NULL;
        CHAN_OUT1(thread_state_t, threads[curr_free_index], new_thread,
            THREAD_ARRAY_CH);
        curr_free_index++;
        return 0;
    }
    return -1;
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
static unsigned get_current() {
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

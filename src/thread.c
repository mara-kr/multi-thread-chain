
/** @file thread.c
 *  @brief Implementation of multi-threading functions
 */

#include "chain.h"
#include "thread.h"

#define MAX_NUM_THREADS 4

typedef struct thread_state_t {
    thread_t thread;
    unsigned active;
} thread_state_t;

struct thread_array {
    SELF_CHAN_FIELD_ARRAY(thread_state_t, threads, MAX_NUM_THREADS);
    SELF_CHAN_FIELD(unsigned, current);
    SELF_CHAN_FIELD(unsigned, num_threads);
};

#define FIELD_INIT_thread_array {\
    SELF_FIELD_ARRAY_INITIALIZER(MAX_NUM_THREADS),\
    SELF_FIELD_INITIALIZER,\
    SELF_FIELD_INITIALIZER\
}

TASK(1, scheduler_task)

// Empty task so we can create a self-channel
void scheduler_task() {
    return;
}


SELF_CHANNEL(scheduler_task, thread_array);

void thread_init() {
    thread_state_t *threads = *CHAN_IN1(thread_state_t *, threads,
            SELF_IN_CH(scheduler_task));
    threads[0].active = 1;

    for (unsigned i = 1; i < MAX_NUM_THREADS; i++) {
        threads[i].active = 0;
    }
}

void thread_end() {
    thread_state_t *threads = *CHAN_IN1(thread_state_t *, threads,
            SELF_IN_CH(scheduler_task));
    unsigned current = *CHAN_IN1(unsigned, current, SELF_CH(scheduler_task));
    threads[current].active = 0;
    CHAN_OUT1(thread_state_t, threads[current], threads[current],
            SELF_OUT_CH(scheduler_task));
}

int thread_create(task_t *new_task) {
    thread_state_t *threads = *CHAN_IN1(thread_state_t *, threads,
            SELF_IN_CH(scheduler_task));
    unsigned num_threads = *CHAN_IN1(unsigned, num_threads,
            SELF_CH(scheduler_task));
    if (num_threads == MAX_NUM_THREADS) { return -1;}
    for (unsigned i = 0; i < MAX_NUM_THREADS; i++) {
        if (!threads[i].active) {
            threads[i].thread.context.task = new_task;
            // TODO Set to creation time instead of 0?
            threads[i].thread.context.time = 0;
            threads[i].thread.context.next_ctx = NULL;
            num_threads++;
            CHAN_OUT1(thread_state_t, threads[i], threads[i],
                    SELF_OUT_CH(scheduler_task));
            CHAN_OUT1(unsigned, num_threads, num_threads,
                    SELF_OUT_CH(scheduler_task));
            return 0;
        }
    }
    return -2;
}

thread_t *get_current_thread() {
    thread_state_t *threads = *CHAN_IN1(thread_state_t *, threads,
            SELF_IN_CH(scheduler_task));
    unsigned current = *CHAN_IN1(unsigned, current,
            SELF_CH(scheduler_task));
    return &threads[current].thread;
}


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
SELF_CHANNEL(scheduler_task, thread_array);

//Dummy function wrapper to get the declarations to play nice
void write_to_scheduler(sch_chan_fields field, void * input){
    thread_state_t thr;
	switch(field){
		case threads:
			CHAN_OUT1(thread_state_t *, threads, *((thread_state_t **)input),
								SELF_OUT_CH(scheduler_task));
			break;
		case thread:
            thr.thread = *((thread_t *) input);
            thr.active = 1; // FIXME
			CHAN_OUT1(thread_state_t, threads[0], thr,
								SELF_OUT_CH(scheduler_task));
			break;
		case current:
			LIBCHAIN_PRINTF("Error- read only variable!\r\n");
			break;
        case num_threads:
			LIBCHAIN_PRINTF("Error- read only variable!\r\n");
			break;
		default:
			break;
	}
}

void read_from_scheduler(sch_chan_fields field, void * output){
	switch(field){
		case threads:
			output = CHAN_IN1(thread_state_t, threads,
							 SELF_IN_CH(scheduler_task));
			break;
		case current:
			*((unsigned *) output) = *CHAN_IN1(unsigned, current,
								SELF_IN_CH(scheduler_task));
			break;
		case num_threads:
			*((unsigned *) output) = *CHAN_IN1(unsigned, num_threads,
								SELF_IN_CH(scheduler_task));
			break;
        case thread:
            LIBCHAIN_PRINTF("Unimplemented\r\n");
            break;
        default:
            break;
	}
	return;
}

// Empty task so we can create a self-channel
void scheduler_task() {
		LIBCHAIN_PRINTF("Inside scheduler task!! \r\n");
		return;
}

/** @brief scheduler initialize the thread_array
*/
void scheduler_init(){
		thread_state_t *thread_states = CHAN_IN1(thread_state_t, threads,
						SELF_IN_CH(scheduler_task));
		//TODO add check for optimization
		for(unsigned i = 0; i < MAX_NUM_THREADS; i++){
				thread_states[i].active = 0;
		}
		//Set the current thread to index 0
		unsigned curthread_index = 0;
		CHAN_OUT1(unsigned, current, curthread_index, SELF_OUT_CH(scheduler_task));
		//Set the number of threads to 1
		//TODO make this not necessarily 1!
		unsigned num_threads = 1;
		CHAN_OUT1(unsigned, num_threads, num_threads, SELF_OUT_CH(scheduler_task));
		LIBCHAIN_PRINTF("Inside scheduler init! \r\n");
		return;
}


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
            SELF_IN_CH(scheduler_task));
    return &threads[current].thread;
}

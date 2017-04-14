
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

//Yes, this is kludgy, but so it goes
__nv unsigned task_init_flag = 0; 

typedef struct thread_state_t {
    thread_t thread;
    unsigned active;
} thread_state_t;

struct thread_array {
    SELF_CHAN_FIELD(unsigned, current);
    SELF_CHAN_FIELD(unsigned, num_threads);
    SELF_CHAN_FIELD_ARRAY(thread_state_t, threads, MAX_NUM_THREADS);
};

#define FIELD_INIT_thread_array {\
    SELF_FIELD_INITIALIZER,\
    SELF_FIELD_INITIALIZER,\
    SELF_FIELD_ARRAY_INITIALIZER(MAX_NUM_THREADS)\
}

static void scheduler_chan_out(const char *field_name, const void *value,
        size_t var_size, uint8_t *chan, size_t field_offset);

#define SCHEDULER_CHAN_OUT(type, field, val, chan0) \
    scheduler_chan_out(#field, &val, sizeof(VAR_TYPE(type)), \
             (uint8_t *) chan0, offsetof(__typeof__(chan0->data), field))

#define SCHEDULER_CHAN_IN(type, field, chan0) \
    CHAN_IN1(type, field, chan0)

// TODO - we should use a different #define so that the
// scheduler task/channel symbols don't conflict and are easy to
// access
TASK(5, scheduler_task)
SELF_CHANNEL(scheduler_task, thread_array);
CHANNEL(task_init, scheduler_task, thread_array); 
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

void scheduler_task() {
    LIBCHAIN_PRINTF("Inside scheduler task!! \r\n");
    task_prologue(); 
    //Yes, I know it's an NV read inside of every scheduler run.. we're working on it. 
    if(task_init_flag){
      task_init_flag = 0; 
	  }
     unsigned current = *CHAN_IN2(unsigned, current, CH(task_init, scheduler_task),
            										SELF_IN_CH(scheduler_task)); 
    LIBCHAIN_PRINTF("scheduler_task Current = %u \r\n", current); 
		//thread_state_t cur_thread = *CHAN_IN2(thread_state_t,threads[current], 
	  //											CH(task_init, scheduler_task), SELF_IN_CH(scheduler_task));
   thread_state_t cur_thread = *CHAN_IN1(thread_state_t, threads[current],
                            SELF_IN_CH(scheduler_task)); 
   LIBCHAIN_PRINTF("Cur thread.id = %u active = %u \r\n", 
	 											cur_thread.thread.thread_id, cur_thread.active); 
	 task_t *next_task = cur_thread.thread.context.task;
   LIBCHAIN_PRINTF("scheduler next task = %x \r\n", next_task); 
    transition_to(next_task);
    return;
}


// Transition to the next task in the current thread
void transition_to_mt(task_t *next_task){
    LIBCHAIN_PRINTF("transition_to_mt \r\n");
    unsigned thread_id = get_current_thread().thread_id;
    LIBCHAIN_PRINTF("Cur thread id = %u \r\n",thread_id); 
		thread_t next_thr;
    thread_state_t next_thr_state;
    context_t next_ctx;

    //Update context passed in
    next_ctx.task = next_task;
    next_ctx.time = curctx->time + 1;
    LIBCHAIN_PRINTF("transition_to_mt next task = %x \r\n",next_ctx.task);
    //Make thread_t to pass to scheduler
    next_thr.thread_id = thread_id;
    next_thr.context = next_ctx;

    //Write thread out to scheduler
    next_thr_state.thread = next_thr;
    next_thr_state.active = 1; // Hasn't called thread_end, still active

    unsigned current = *SCHEDULER_CHAN_IN(unsigned, current,
            						SELF_IN_CH(scheduler_task));
    LIBCHAIN_PRINTF("transition_to_mt Current = %x \r\n", current);  
		SCHEDULER_CHAN_OUT(thread_state_t, threads[current], next_thr_state,
            SELF_OUT_CH(scheduler_task));
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

/** @brief scheduler initialize the thread_array
*/
void scheduler_init(){
    thread_state_t thread;
    LIBCHAIN_PRINTF("Inside scheduler init! \r\n");
    //Not worried about writing to NV memory here b/c it'll get used in transition_to_mt
    //which can't be called until AFTER this finishes b/c of the ordering of functions in
    //tasks. 
    task_init_flag = 1; 
    //TODO add check for optimization
		//Assume thread_init set all these to 0? 
		/*for (unsigned i = 0; i < MAX_NUM_THREADS; i++) {
       // thread = *SCHEDULER_CHAN_IN(thread_state_t, threads[i],
       //         SELF_IN_CH(scheduler_task));
       	 thread.active = 0;
        CHAN_OUT1(thread_state_t, threads[i], thread,
                CH(task_init, scheduler_task));
    }*/

    //Set the current thread to index 0
    unsigned current = 0;
    SCHEDULER_CHAN_OUT(unsigned, current, current,
            SELF_OUT_CH(scheduler_task));
		//CHAN_OUT1(unsigned, current, current,CH(task_init, scheduler_task)); 
    //Set the number of threads to 1 (just the current running thread)
    unsigned num_threads = 1;
    //SCHEDULER_CHAN_OUT(unsigned, num_threads, num_threads,
    //        SELF_OUT_CH(scheduler_task));
		CHAN_OUT1(unsigned, num_threads, num_threads, CH(task_init, scheduler_task)); 
    swap_scheduler_buffer();  
		return;
}


void thread_init() {
    thread_state_t thr;
    thr.thread.context = *curctx;  
		thr.thread.thread_id = 0;
    thr.active = 1;
    CHAN_OUT1(thread_state_t, threads[0], thr,CH(task_init, scheduler_task));

    thread_state_t threads[MAX_NUM_THREADS];
    threads[0].active = 1;
		
		LIBCHAIN_PRINTF("Inside thread init! \r\n"); 
    for (unsigned i = 1; i < MAX_NUM_THREADS; i++) {
        threads[i].active = 0;
    }
    for (unsigned i = 1; i < MAX_NUM_THREADS; i++) {
        CHAN_OUT1(thread_state_t, threads[i], threads[i],
                CH(task_init, scheduler_task));
    }
    //scheduler_init();
}


void thread_end() {
    unsigned current = *SCHEDULER_CHAN_IN(unsigned, current, SELF_IN_CH(scheduler_task));
    thread_state_t curthreadst = *SCHEDULER_CHAN_IN(thread_state_t , threads[current],
            																				SELF_IN_CH(scheduler_task));
		curthreadst.active = 0;
    SCHEDULER_CHAN_OUT(thread_state_t, threads[current], curthreadst,
            SELF_OUT_CH(scheduler_task));
    // TODO - needs to transition to the next availiable task
}


int thread_create(task_t *new_task) {
    thread_state_t curthreadst;
    unsigned num_threads = *SCHEDULER_CHAN_IN(unsigned, num_threads,
            SELF_CH(scheduler_task));
    if (num_threads == MAX_NUM_THREADS) { return -1;}
    for (unsigned i = 0; i < MAX_NUM_THREADS; i++) {
        curthreadst = *SCHEDULER_CHAN_IN(thread_state_t, threads[i], 
															SELF_IN_CH(scheduler_task)); 
				if (!curthreadst.active) {
            curthreadst.thread.context.task = new_task;
            // TODO Set to creation time instead of 0?
            curthreadst.thread.context.time = 0;
            curthreadst.thread.context.next_ctx = NULL;
            num_threads++;
            SCHEDULER_CHAN_OUT(thread_state_t, threads[i], curthreadst, 
                    SELF_OUT_CH(scheduler_task));
            SCHEDULER_CHAN_OUT(unsigned, num_threads, num_threads,
                    SELF_OUT_CH(scheduler_task));
            
						return 0;
        }
    }
    return -2;
}

thread_t get_current_thread() {
    unsigned current = *CHAN_IN2(unsigned, current,CH(task_init, scheduler_task),
            SELF_IN_CH(scheduler_task));
    LIBCHAIN_PRINTF("current = %x\r\n", current);
    thread_state_t thread = *CHAN_IN2(thread_state_t, threads[current],
												CH(task_init, scheduler_task), SELF_IN_CH(scheduler_task));
    return thread.thread;
}

/** @file scheduler.h
 *  @brief TODO
 *
 *
 */

#ifndef _SCHEDULER_H
#define _SCHEDULER_H

#include <chain.h>

typedef struct thread_t {
    // TODO - overflow is possible!
    unsigned thread_id;
    context_t context;
}

/** @brief Initialize scheduler */
void thread_init();

/** @brief TODO
 */
void thread_end();

/** @brief TODO */
int thread_create(task_t *new_task);

/** @brief TODO */
thread_t * volatile get_current_thread();

#endif

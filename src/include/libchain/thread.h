/** @file scheduler.h
 *  @brief TODO
 *
 *
 */

#ifndef _SCHEDULER_H
#define _SCHEDULER_H

#include <chain.h>

__nv extern thread_t * volatile cur_thread; 


typedef struct thread_t {
    // TODO - overflow is possible!
    unsigned thread_id;
    context_t context;
}

/** @brief TODO
 */
void thread_end();

/** @brief TODO */
int thread_create();

/** @brief returns a pointer to the current thread */ 
uint8_t getThreadPtr(); 

#endif

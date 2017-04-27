/** @file Implementation of intermittant mutexes
 *
 *  This implementation requires that scheduling occurs on
 *  transitions, not on reboots
 */

#include "mutex.h"
#include "thread.h"

int mutex_init(mutex_t *m) {
    if (m == NULL) { return -1;}
    m->free = 1;
    m->holder = MAX_NUM_THREADS + 1; 
    return 0;
}

void mutex_lock(mutex_t *m, unsigned id) {
    while (1) {
        if (m->holder!=id && !m->free) {
            deschedule();
        } else {
            LIBCHAIN_PRINTF("Got lock!\r\n"); 
            m->free = 0;
            m->holder = id; 
            break;
        }
    }
}

void mutex_unlock(mutex_t *m) {
    m->free = 1;
}

void mutex_destroy(mutex_t *m) {
    m->free = 0;
}

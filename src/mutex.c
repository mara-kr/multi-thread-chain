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
    return 0;
}

void mutex_lock(mutex_t *m) {
    while (1) {
        if (!m->free) {
            deschedule();
        } else {
            m->free = 0;
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

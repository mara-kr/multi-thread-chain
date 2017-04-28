/** @file Implementation of intermittant mutexes
 *
 *  This implementation requires that scheduling occurs on
 *  transitions, not on reboots
 */
#include <stdarg.h>
#include <string.h>

#ifndef LIBCHAIN_ENABLE_DIAGNOSTICS
#define LIBCHAIN_PRINTF(...)
#else
#include <stdio.h>
#define LIBCHAIN_PRINTF printf
#endif


#include "mutex.h"
#include "thread.h"

int mutex_init(mutex_t *m) {
    if (m == NULL) { return -1;}
    m->free = 1;
    m->holder = MAX_NUM_THREADS + 1; 
    return 0;
}

void mutex_lock(mutex_t *m) {
    while (1) {
        unsigned id = get_current(); 
        LIBCHAIN_PRINTF("Holder id = %u Free = %u \r\n", m->holder, m->free); 
        if (m->holder!=id && !m->free) {
            LIBCHAIN_PRINTF("No lock for you! \r\n"); 
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
    LIBCHAIN_PRINTF("Freeing lock!! \r\n"); 
    m->free = 1;
}

void mutex_destroy(mutex_t *m) {
    m->free = 0;
}

/** @file Interface for intermittant mutexes
 */

#ifndef _MUTEX_H
#define _MUTEX_H

typedef struct mutex_t {
    unsigned free;
    unsigned holder; 
} mutex_t;

/** @brief Initialize the mutex pointed to by m
 *  @param m Mutex to initialize
 *  @return 0 on success, a negative integer on error
 */
int mutex_init(mutex_t *m);

/** @brief Attempt to lock mutex m
 *  @param m Mutex to lock
 *  @param id thread id of holder
 *  @return Void
 *
 *  If the mutex cannot be locked, deschedule the calling thread
 */
void mutex_lock(mutex_t *m);

/** @brief Unlock mutex m
 *  @param m Mutex to unlock
 *  @return Void
 */
void mutex_unlock(mutex_t *m);

/** @brief Destroy mutex pointed to by m
 *  @param m Mutex to destroy
 *  @return Void
 */
void mutex_destroy(mutex_t *m);

#endif

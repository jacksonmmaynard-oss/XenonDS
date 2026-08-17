// SPDX-License-Identifier: MIT
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <rthreads/rthreads.h>

// v0.3 runs DeSmuME on one Xenon hardware thread. DeSmuME's Task adapter is
// patched to execute work synchronously, so these objects only provide the
// allocation/lifetime surface expected by the core.
struct sthread { int unused; };
struct slock { int unused; };
struct scond { int unused; };

sthread_t* sthread_create(void (*thread_func)(void*), void* userdata) {
    (void)thread_func;
    (void)userdata;
    return (sthread_t*)calloc(1, sizeof(sthread_t));
}

sthread_t* sthread_create_with_priority(void (*thread_func)(void*),
                                       void* userdata,
                                       int thread_priority) {
    (void)thread_priority;
    return sthread_create(thread_func, userdata);
}

int sthread_detach(sthread_t* thread) { free(thread); return 0; }
void sthread_join(sthread_t* thread) { free(thread); }
bool sthread_isself(sthread_t* thread) { (void)thread; return true; }
void sthread_setname(sthread_t* thread, const char* name) {
    (void)thread;
    (void)name;
}

slock_t* slock_new(void) { return (slock_t*)calloc(1, sizeof(slock_t)); }
void slock_free(slock_t* lock) { free(lock); }
void slock_lock(slock_t* lock) { (void)lock; }
void slock_unlock(slock_t* lock) { (void)lock; }

scond_t* scond_new(void) { return (scond_t*)calloc(1, sizeof(scond_t)); }
void scond_free(scond_t* cond) { free(cond); }
void scond_wait(scond_t* cond, slock_t* lock) { (void)cond; (void)lock; }
bool scond_wait_timeout(scond_t* cond, slock_t* lock, int64_t timeout_us) {
    (void)cond;
    (void)lock;
    (void)timeout_us;
    return false;
}
int scond_broadcast(scond_t* cond) { (void)cond; return 0; }
void scond_signal(scond_t* cond) { (void)cond; }

#ifndef __QEMU_THREAD_POSIX_H
#define __QEMU_THREAD_POSIX_H 1

#include "pthread.h"
#include <semaphore.h>
#include <mach/semaphore.h>

struct QemuMutex {
    pthread_mutex_t lock;
};

struct QemuCond {
    pthread_cond_t cond;
};

struct QemuSemaphore {
    semaphore_t sem;
};

struct QemuEvent {
    pthread_mutex_t lock;
    pthread_cond_t cond;

    unsigned value;
};

struct QemuThread {
    pthread_t thread;
};

struct RFifoLock {
    QemuMutex lock;             /* protects all fields */
    
    /* FIFO order */
    unsigned int head;          /* active ticket number */
    unsigned int tail;          /* waiting ticket number */
    QemuCond cond;              /* used to wait for our ticket number */
    
    /* Nesting */
    QemuThread owner_thread;    /* thread that currently has ownership */
    unsigned int nesting;       /* amount of nesting levels */
    
    /* Contention callback */
    void (*cb)(void *);         /* called when thread must wait, with ->lock
                                 * held so it may not recursively lock/unlock
                                 */
    void *cb_opaque;
};

#endif

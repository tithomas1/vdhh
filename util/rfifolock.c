/*
 * Recursive FIFO lock
 *
 * Copyright Red Hat, Inc. 2013
 *
 * Authors:
 *  Stefan Hajnoczi   <stefanha@redhat.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2 or later.
 * See the COPYING.LIB file in the top-level directory.
 *
 */

#include <assert.h>
#include "qemu/thread.h"

void rfifolock_init(RFifoLock *r, void (*cb)(void *), void *opaque)
{
    vmx_mutex_init(&r->lock);
    r->head = 0;
    r->tail = 0;
    vmx_cond_init(&r->cond);
    r->nesting = 0;
    r->cb = cb;
    r->cb_opaque = opaque;
}

void rfifolock_destroy(RFifoLock *r)
{
    vmx_cond_destroy(&r->cond);
    vmx_mutex_destroy(&r->lock);
}

/*
 * Theory of operation:
 *
 * In order to ensure FIFO ordering, implement a ticketlock.  Threads acquiring
 * the lock enqueue themselves by incrementing the tail index.  When the lock
 * is unlocked, the head is incremented and waiting threads are notified.
 *
 * Recursive locking does not take a ticket since the head is only incremented
 * when the outermost recursive caller unlocks.
 */
void rfifolock_lock(RFifoLock *r)
{
    vmx_mutex_lock(&r->lock);

    /* Take a ticket */
    unsigned int ticket = r->tail++;

    if (r->nesting > 0 && vmx_thread_is_self(&r->owner_thread)) {
        r->tail--; /* put ticket back, we're nesting */
    } else {
        while (ticket != r->head) {
            /* Invoke optional contention callback */
            if (r->cb) {
                r->cb(r->cb_opaque);
            }
            vmx_cond_wait(&r->cond, &r->lock);
        }
    }

    vmx_thread_get_self(&r->owner_thread);
    r->nesting++;
    vmx_mutex_unlock(&r->lock);
}

void rfifolock_unlock(RFifoLock *r)
{
    vmx_mutex_lock(&r->lock);
    assert(r->nesting > 0);
    assert(vmx_thread_is_self(&r->owner_thread));
    if (--r->nesting == 0) {
        r->head++;
        vmx_cond_broadcast(&r->cond);
    }
    vmx_mutex_unlock(&r->lock);
}

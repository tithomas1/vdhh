/*
 * coroutine queues and locks
 *
 * Copyright (c) 2011 Kevin Wolf <kwolf@redhat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu-common.h"
#include "coroutine.h"
#include "coroutine_int.h"
#include "qemu/queue.h"

void vmx_co_queue_init(CoQueue *queue)
{
    QTAILQ_INIT(&queue->entries);
}

void coroutine_fn vmx_co_queue_wait(CoQueue *queue)
{
    Coroutine *self = vmx_coroutine_self();
    QTAILQ_INSERT_TAIL(&queue->entries, self, co_queue_next);
    vmx_coroutine_yield();
    assert(vmx_in_coroutine());
}

/**
 * vmx_co_queue_run_restart:
 *
 * Enter each coroutine that was previously marked for restart by
 * vmx_co_queue_next() or vmx_co_queue_restart_all().  This function is
 * invoked by the core coroutine code when the current coroutine yields or
 * terminates.
 */
void vmx_co_queue_run_restart(Coroutine *co)
{
    Coroutine *next;

    while ((next = QTAILQ_FIRST(&co->co_queue_wakeup))) {
        QTAILQ_REMOVE(&co->co_queue_wakeup, next, co_queue_next);
        vmx_coroutine_enter(next, NULL);
    }
}

static bool vmx_co_queue_do_restart(CoQueue *queue, bool single)
{
    Coroutine *self = vmx_coroutine_self();
    Coroutine *next;

    if (QTAILQ_EMPTY(&queue->entries)) {
        return false;
    }

    while ((next = QTAILQ_FIRST(&queue->entries)) != NULL) {
        QTAILQ_REMOVE(&queue->entries, next, co_queue_next);
        QTAILQ_INSERT_TAIL(&self->co_queue_wakeup, next, co_queue_next);
        if (single) {
            break;
        }
    }
    return true;
}

bool coroutine_fn vmx_co_queue_next(CoQueue *queue)
{
    assert(vmx_in_coroutine());
    return vmx_co_queue_do_restart(queue, true);
}

void coroutine_fn vmx_co_queue_restart_all(CoQueue *queue)
{
    assert(vmx_in_coroutine());
    vmx_co_queue_do_restart(queue, false);
}

bool vmx_co_enter_next(CoQueue *queue)
{
    Coroutine *next;

    next = QTAILQ_FIRST(&queue->entries);
    if (!next) {
        return false;
    }

    QTAILQ_REMOVE(&queue->entries, next, co_queue_next);
    vmx_coroutine_enter(next, NULL);
    return true;
}

bool vmx_co_queue_empty(CoQueue *queue)
{
    return (QTAILQ_FIRST(&queue->entries) == NULL);
}

void vmx_co_mutex_init(CoMutex *mutex)
{
    memset(mutex, 0, sizeof(*mutex));
    vmx_co_queue_init(&mutex->queue);
}

void coroutine_fn vmx_co_mutex_lock(CoMutex *mutex)
{
    Coroutine *self = vmx_coroutine_self();

    while (mutex->locked) {
        vmx_co_queue_wait(&mutex->queue);
    }

    mutex->locked = true;
}

void coroutine_fn vmx_co_mutex_unlock(CoMutex *mutex)
{
    Coroutine *self = vmx_coroutine_self();

    assert(mutex->locked == true);
    assert(vmx_in_coroutine());

    mutex->locked = false;
    vmx_co_queue_next(&mutex->queue);
}

void vmx_co_rwlock_init(CoRwlock *lock)
{
    memset(lock, 0, sizeof(*lock));
    vmx_co_queue_init(&lock->queue);
}

void vmx_co_rwlock_rdlock(CoRwlock *lock)
{
    while (lock->writer) {
        vmx_co_queue_wait(&lock->queue);
    }
    lock->reader++;
}

void vmx_co_rwlock_unlock(CoRwlock *lock)
{
    assert(vmx_in_coroutine());
    if (lock->writer) {
        lock->writer = false;
        vmx_co_queue_restart_all(&lock->queue);
    } else {
        lock->reader--;
        assert(lock->reader >= 0);
        /* Wakeup only one waiting writer */
        if (!lock->reader) {
            vmx_co_queue_next(&lock->queue);
        }
    }
}

void vmx_co_rwlock_wrlock(CoRwlock *lock)
{
    while (lock->writer || lock->reader) {
        vmx_co_queue_wait(&lock->queue);
    }
    lock->writer = true;
}

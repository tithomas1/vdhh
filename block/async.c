/*
 * QEMU System Emulator
 *
 * Copyright (c) 2003-2008 Fabrice Bellard
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
#include "aio.h"
#include "thread-pool.h"
#include "qemu/main-loop.h"
#include "qemu/atomic.h"

/***********************************************************/
/* bottom halves (can be seen as timers which expire ASAP) */

struct QEMUBH {
    VeertuAioContext *ctx;
    QEMUBHFunc *cb;
    void *opaque;
    QEMUBH *next;
    bool scheduled;
    bool idle;
    bool deleted;
};

struct VeertuAioHandler
{
    GPollFD pfd;
    IOHandler *io_read;
    IOHandler *io_write;
    int deleted;
    int pollfds_idx;
    void *opaque;
    QLIST_ENTRY(VeertuAioHandler) node;
};

struct VeertuAioContext
{
    GSource source;

    /* Protects all fields from multi-threaded access */
    RFifoLock lock;

    /* The list of registered AIO handlers */
    QLIST_HEAD(, VeertuAioHandler) aio_handlers;

    /* This is a simple lock used to protect the aio_handlers list.
     * Specifically, it's used to ensure that no callbacks are removed while
     * we're walking and dispatching callbacks.
     */
    int walking_handlers;

    /* Used to avoid unnecessary veertu_event_notifier_set calls in aio_notify.
     * Writes protected by lock or BQL, reads are lockless.
     */
    bool dispatching;

    /* lock to protect between bh's adders and deleter */
    QemuMutex bh_lock;

    /* Anchor of the list of Bottom Halves belonging to the context */
    struct QEMUBH *first_bh;

    /* A simple lock used to protect the first_bh list, and ensure that
     * no callbacks are removed while we're walking and dispatching callbacks.
     */
    int walking_bh;

    /* Used for aio_notify.  */
    EventNotifier *notifier;

    /* GPollFDs for aio_poll() */
    GArray *pollfds;

    /* Thread pool for performing work and receiving completion callbacks */
    struct ThreadPool *thread_pool;

    /* TimerLists for calling timers - one per clock type */
    QEMUTimerListGroup tlg;
};


QEMUBH *aio_bh_new(VeertuAioContext *ctx, QEMUBHFunc *cb, void *opaque)
{
    QEMUBH *bh;
    bh = g_new(QEMUBH, 1);
    *bh = (QEMUBH){
        .ctx = ctx,
        .cb = cb,
        .opaque = opaque,
    };
    vmx_mutex_lock(&ctx->bh_lock);
    bh->next = ctx->first_bh;
    /* Make sure that the members are ready before putting bh into list */
    smp_wmb();
    ctx->first_bh = bh;
    vmx_mutex_unlock(&ctx->bh_lock);
    return bh;
}

/* Multiple occurrences of aio_bh_poll cannot be called concurrently */
int aio_bh_poll(VeertuAioContext *ctx)
{
    QEMUBH *bh, **bhp, *next;
    int ret;

    ctx->walking_bh++;

    ret = 0;
    for (bh = ctx->first_bh; bh; bh = next) {
        /* Make sure that fetching bh happens before accessing its members */
        smp_read_barrier_depends();
        next = bh->next;
        if (!bh->deleted && bh->scheduled) {
            bh->scheduled = 0;
            /* Paired with write barrier in bh schedule to ensure reading for
             * idle & callbacks coming after bh's scheduling.
             */
            smp_rmb();
            if (!bh->idle)
                ret = 1;
            bh->idle = 0;
            bh->cb(bh->opaque);
        }
    }

    ctx->walking_bh--;

    /* remove deleted bhs */
    if (!ctx->walking_bh) {
        vmx_mutex_lock(&ctx->bh_lock);
        bhp = &ctx->first_bh;
        while (*bhp) {
            bh = *bhp;
            if (bh->deleted) {
                *bhp = bh->next;
                g_free(bh);
            } else {
                bhp = &bh->next;
            }
        }
        vmx_mutex_unlock(&ctx->bh_lock);
    }

    return ret;
}

void vmx_bh_schedule_idle(QEMUBH *bh)
{
    if (bh->scheduled)
        return;
    bh->idle = 1;
    /* Make sure that idle & any writes needed by the callback are done
     * before the locations are read in the aio_bh_poll.
     */
    smp_wmb();
    bh->scheduled = 1;
}

void vmx_bh_schedule(QEMUBH *bh)
{
    VeertuAioContext *ctx;

    if (bh->scheduled)
        return;
    ctx = bh->ctx;
    bh->idle = 0;
    /* Make sure that:
     * 1. idle & any writes needed by the callback are done before the
     *    locations are read in the aio_bh_poll.
     * 2. ctx is loaded before scheduled is set and the callback has a chance
     *    to execute.
     */
    smp_mb();
    bh->scheduled = 1;
    aio_notify(ctx);
}


/* This func is async.
 */
void vmx_bh_cancel(QEMUBH *bh)
{
    bh->scheduled = 0;
}

/* This func is async.The bottom half will do the delete action at the finial
 * end.
 */
void vmx_bh_delete(QEMUBH *bh)
{
    bh->scheduled = 0;
    bh->deleted = 1;
}

int64_t
aio_compute_timeout(VeertuAioContext *ctx)
{
    int64_t deadline;
    int timeout = -1;
    QEMUBH *bh;

    for (bh = ctx->first_bh; bh; bh = bh->next) {
        if (!bh->deleted && bh->scheduled) {
            if (bh->idle) {
                /* idle bottom halves will be polled at least
                 * every 10ms */
                timeout = 10000000;
            } else {
                /* non-idle bottom halves will be executed
                 * immediately */
                return 0;
            }
        }
    }

    deadline = timerlistgroup_deadline_ns(&ctx->tlg);
    if (deadline == 0) {
        return 0;
    } else {
        return vmx_soonest_timeout(timeout, deadline);
    }
}

static gboolean
aio_ctx_prepare(GSource *source, gint    *timeout)
{
    VeertuAioContext *ctx = (VeertuAioContext *) source;

    /* We assume there is no timeout already supplied */
    *timeout = vmx_timeout_ns_to_ms(aio_compute_timeout(ctx));

    if (aio_prepare(ctx)) {
        *timeout = 0;
    }

    return *timeout == 0;
}

static gboolean
aio_ctx_check(GSource *source)
{
    VeertuAioContext *ctx = (VeertuAioContext *) source;
    QEMUBH *bh;

    for (bh = ctx->first_bh; bh; bh = bh->next) {
        if (!bh->deleted && bh->scheduled) {
            return true;
	}
    }
    return aio_pending(ctx) || (timerlistgroup_deadline_ns(&ctx->tlg) == 0);
}

static bool aio_dispatch(VeertuAioContext *ctx);

static gboolean
aio_ctx_dispatch(GSource     *source,
                 GSourceFunc  callback,
                 gpointer     user_data)
{
    VeertuAioContext *ctx = (VeertuAioContext *) source;

    assert(callback == NULL);
    aio_dispatch(ctx);
    return true;
}

static void
aio_ctx_finalize(GSource     *source)
{
    VeertuAioContext *ctx = (VeertuAioContext *) source;

    thread_pool_destroy(ctx->thread_pool);
    aio_set_veertu_event_notifier(ctx, ctx->notifier, NULL);
    veertu_event_notifier_destroy(ctx->notifier);
    rfifolock_destroy(&ctx->lock);
    vmx_mutex_destroy(&ctx->bh_lock);
    g_array_free(ctx->pollfds, TRUE);
    timerlistgroup_deinit(&ctx->tlg);
}

static GSourceFuncs aio_source_funcs = {
    aio_ctx_prepare,
    aio_ctx_check,
    aio_ctx_dispatch,
    aio_ctx_finalize
};

GSource *aio_get_g_source(VeertuAioContext *ctx)
{
    g_source_ref(&ctx->source);
    return &ctx->source;
}

ThreadPool *aio_get_thread_pool(VeertuAioContext *ctx)
{
    if (!ctx->thread_pool) {
        ctx->thread_pool = thread_pool_create(ctx, 10);
    }
    return ctx->thread_pool;
}

void aio_set_dispatching(VeertuAioContext *ctx, bool dispatching)
{
    ctx->dispatching = dispatching;
    if (!dispatching) {
        /* Write ctx->dispatching before reading e.g. bh->scheduled.
         * Optimization: this is only needed when we're entering the "unsafe"
         * phase where other threads must call veertu_event_notifier_set.
         */
        smp_mb();
    }
}

void aio_notify(VeertuAioContext *ctx)
{
    /* Write e.g. bh->scheduled before reading ctx->dispatching.  */
    smp_mb();
    if (!ctx->dispatching) {
        veertu_event_notifier_set(ctx->notifier);
    }
}

static void aio_timerlist_notify(void *opaque)
{
    aio_notify(opaque);
}

static void aio_rfifolock_cb(void *opaque)
{
    /* Kick owner thread in case they are blocked in aio_poll() */
    aio_notify(opaque);
}

VeertuAioContext *aio_context_new(Error **errp)
{
    int ret;
    VeertuAioContext *ctx;
    ctx = (VeertuAioContext *) g_source_new(&aio_source_funcs, sizeof(VeertuAioContext));
    ctx->notifier = veertu_event_notifier_create(false);
    g_source_set_can_recurse(&ctx->source, true);
    aio_set_veertu_event_notifier(ctx, ctx->notifier,
                           (EventNotifierHandler *)
                           veertu_event_notifier_test_and_clear);
    ctx->pollfds = g_array_new(FALSE, FALSE, sizeof(GPollFD));
    ctx->thread_pool = NULL;
    vmx_mutex_init(&ctx->bh_lock);
    rfifolock_init(&ctx->lock, aio_rfifolock_cb, ctx);
    timerlistgroup_init(&ctx->tlg, aio_timerlist_notify, ctx);

    return ctx;
}

void aio_context_ref(VeertuAioContext *ctx)
{
    g_source_ref(&ctx->source);
}

void aio_context_unref(VeertuAioContext *ctx)
{
    g_source_unref(&ctx->source);
}

void aio_context_acquire(VeertuAioContext *ctx)
{
    rfifolock_lock(&ctx->lock);
}

void aio_context_release(VeertuAioContext *ctx)
{
    rfifolock_unlock(&ctx->lock);
}

void aio_set_fd_handler(VeertuAioContext *ctx,
                        int fd,
                        IOHandler *io_read,
                        IOHandler *io_write,
                        void *opaque)
{
    struct VeertuAioHandler *node;
    
    QLIST_FOREACH(node, &ctx->aio_handlers, node) {
        if (node->pfd.fd == fd && !node->deleted)
            break;
    }

    if (io_read || io_write) {
        if (node == NULL) {
            node = g_new0(struct VeertuAioHandler, 1);
            node->pfd.fd = fd;
            QLIST_INSERT_HEAD(&ctx->aio_handlers, node, node);

            g_source_add_poll(&ctx->source, &node->pfd);
        }

        node->io_read = io_read;
        node->io_write = io_write;
        node->opaque = opaque;
        node->pollfds_idx = -1;
        node->pfd.events = 0;

        if (io_read)
            node->pfd.events |= G_IO_IN | G_IO_HUP | G_IO_ERR;
        if (io_write)
            node->pfd.events |= G_IO_OUT | G_IO_ERR;
    }
    else {
        if (node) {
            g_source_remove_poll(&ctx->source, &node->pfd);

            /* If the lock is held, just mark the node as deleted */
            if (ctx->walking_handlers) {
                node->deleted = 1;
                node->pfd.revents = 0;
            } else {
                /* Otherwise, delete it for real.  We can't just mark it as
                 * deleted because deleted nodes are only cleaned up after
                 * releasing the walking_handlers lock.
                 */
                QLIST_REMOVE(node, node);
                g_free(node);
            }
        }
    }
    aio_notify(ctx);
}

void aio_set_veertu_event_notifier(VeertuAioContext *ctx,
                            EventNotifier *notifier,
                            EventNotifierHandler *io_read)
{
    aio_set_fd_handler(ctx, veertu_event_notifier_get_fd(notifier),
                       (IOHandler *)io_read, NULL, notifier);
}

bool aio_prepare(VeertuAioContext *ctx)
{
    return false;
}

bool aio_pending(VeertuAioContext *ctx)
{
    struct VeertuAioHandler *node;

    QLIST_FOREACH(node, &ctx->aio_handlers, node) {
        int revents = node->pfd.revents & node->pfd.events;
        if (revents & (G_IO_IN | G_IO_HUP | G_IO_ERR) && node->io_read)
            return true;
        if (revents & (G_IO_OUT | G_IO_ERR) && node->io_write)
            return true;
    }
    return false;
}

static bool aio_dispatch(VeertuAioContext *ctx)
{
    struct VeertuAioHandler *node;
    bool progress = false;

    /*
     * If there are callbacks left that have been queued, we need to call them.
     * Do not call select in this case, because it is possible that the caller
     * does not need a complete flush (as is the case for aio_poll loops).
     */
    if (aio_bh_poll(ctx)) {
        progress = true;
    }

    /*
     * We have to walk very carefully in case aio_set_fd_handler is
     * called while we're walking.
     */
    node = QLIST_FIRST(&ctx->aio_handlers);
    while (node) {
        struct VeertuAioHandler *tmp;
        int revents;

        ctx->walking_handlers++;

        revents = node->pfd.revents & node->pfd.events;
        node->pfd.revents = 0;

        if (!node->deleted &&
            (revents & (G_IO_IN | G_IO_HUP | G_IO_ERR)) &&
            node->io_read) {
            node->io_read(node->opaque);

            /* aio_notify() does not count as progress */
            if (node->opaque != &ctx->notifier) {
                progress = true;
            }
        }
        if (!node->deleted &&
            (revents & (G_IO_OUT | G_IO_ERR)) &&
            node->io_write) {
            node->io_write(node->opaque);
            progress = true;
        }

        tmp = node;
        node = QLIST_NEXT(node, node);

        ctx->walking_handlers--;

        if (!ctx->walking_handlers && tmp->deleted) {
            QLIST_REMOVE(tmp, node);
            g_free(tmp);
        }
    }
    /* Run our timers */
    progress |= timerlistgroup_run_timers(&ctx->tlg);
    return progress;
}

bool aio_poll(VeertuAioContext *ctx, bool blocking)
{
    struct VeertuAioHandler *node;
    bool was_dispatching;
    int ret;
    bool progress;

    was_dispatching = ctx->dispatching;
    progress = false;

    /* aio_notify can avoid the expensive veertu_event_notifier_set if
     * everything (file descriptors, bottom halves, timers) will
     * be re-evaluated before the next blocking poll().  This is
     * already true when aio_poll is called with blocking == false;
     * if blocking == true, it is only true after poll() returns.
     *
     * If we're in a nested event loop, ctx->dispatching might be true.
     * In that case we can restore it just before returning, but we
     * have to clear it now.
     */
    aio_set_dispatching(ctx, !blocking);

    ctx->walking_handlers++;

    g_array_set_size(ctx->pollfds, 0);

    /* fill pollfds */
    QLIST_FOREACH(node, &ctx->aio_handlers, node) {
        node->pollfds_idx = -1;
        if (!node->deleted && node->pfd.events) {
            GPollFD pfd = {
                .fd = node->pfd.fd,
                .events = node->pfd.events,
            };
            node->pollfds_idx = ctx->pollfds->len;
            g_array_append_val(ctx->pollfds, pfd);
        }
    }

    ctx->walking_handlers--;

    /* wait until next event */
    ret = vmx_poll_ns((GPollFD *)ctx->pollfds->data,
                       ctx->pollfds->len,
                       blocking ? aio_compute_timeout(ctx) : 0);

    /* if we have any readable fds, dispatch event */
    if (ret > 0) {
        QLIST_FOREACH(node, &ctx->aio_handlers, node) {
            if (node->pollfds_idx != -1) {
                GPollFD *pfd = &g_array_index(ctx->pollfds, GPollFD,
                                              node->pollfds_idx);
                node->pfd.revents = pfd->revents;
            }
        }
    }

    /* Run dispatch even if there were no readable fds to run timers */
    aio_set_dispatching(ctx, true);
    if (aio_dispatch(ctx)) {
        progress = true;
    }

    aio_set_dispatching(ctx, was_dispatching);
    return progress;
}

QEMUTimer *aio_timer_new(VeertuAioContext *ctx, QEMUClockType type,
                         int scale,
                         QEMUTimerCB *cb, void *opaque)
{
    return timer_new_tl(ctx->tlg.tl[type], scale, cb, opaque);
}



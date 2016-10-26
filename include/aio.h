#ifndef QEMU_AIO_H
#define QEMU_AIO_H

#include "qemu/typedefs.h"
#include "qemu-common.h"
#include "qemu/queue.h"
#include "qemu/event_notifier.h"
#include "qemu/thread.h"
#include "qemu/timer.h"
#include "qemu/veertu-aio.h"

typedef struct AioHandler AioHandler;
typedef void QEMUBHFunc(void *opaque);
typedef void IOHandler(void *opaque);

typedef struct VeertuAioContext VeertuAioContext;

/**
 * aio_context_new: Allocate a new VeertuAioContext.
 */
VeertuAioContext *aio_context_new(Error **errp);

/* Take ownership of the VeertuAioContext.  If the VeertuAioContext will be shared between
* threads, a thread must have ownership when calling aio_poll().
*/
void aio_context_acquire(VeertuAioContext *ctx);

/* Relinquish ownership of the VeertuAioContext. */
void aio_context_release(VeertuAioContext *ctx);

/**
 * aio_bh_new: Allocate a new bottom half structure.
 */
QEMUBH *aio_bh_new(VeertuAioContext *ctx, QEMUBHFunc *cb, void *opaque);

/**
 * aio_notify: Force processing of pending events.
 */
void aio_notify(VeertuAioContext *ctx);

/**
 * aio_bh_poll: Poll bottom halves for an VeertuAioContext.
 */
int aio_bh_poll(VeertuAioContext *ctx);

/* Progress in completing AIO work to occur.  This can issue new pending
 * aio as a result of executing I/O completion or bh callbacks.
 */
bool aio_poll(VeertuAioContext *ctx, bool blocking);

/* Register a file descriptor and associated callbacks.  Behaves very similarly
 * to vmx_set_fd_handler2.  Unlike vmx_set_fd_handler2, these callbacks will
 * be invoked when using aio_poll().
 */
void aio_set_fd_handler(VeertuAioContext *ctx,
                        int fd,
                        IOHandler *io_read,
                        IOHandler *io_write,
                        void *opaque);

/* Register an event notifier and associated callbacks.  Behaves very similarly
 * to veertu_event_notifier_set_handler.  Unlike veertu_event_notifier_set_handler, these callbacks
 * will be invoked when using aio_poll().
 *
*/
void aio_set_veertu_event_notifier(VeertuAioContext *ctx,
                            EventNotifier *notifier,
                            EventNotifierHandler *io_read);

/* Return a GSource that lets the main loop poll the file descriptors attached
 * to this VeertuAioContext.
 */
GSource *aio_get_g_source(VeertuAioContext *ctx);

bool aio_prepare(VeertuAioContext *ctx);
bool aio_pending(VeertuAioContext *ctx);

/* Return the ThreadPool bound to this VeertuAioContext */
struct ThreadPool *aio_get_thread_pool(VeertuAioContext *ctx);

QEMUTimer *aio_timer_new(VeertuAioContext *ctx, QEMUClockType type,
                        int scale,
                        QEMUTimerCB *cb, void *opaque);
void aio_timer_init(VeertuAioContext *ctx,
                    QEMUTimer *ts, QEMUClockType type,
                    int scale,
                    QEMUTimerCB *cb, void *opaque);

int64_t aio_compute_timeout(VeertuAioContext *ctx);

#endif

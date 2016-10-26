#include "qemu-common.h"
#include "qemu/queue.h"
#include "qemu/thread.h"
#include "qemu/osdep.h"
#include "coroutine.h"
#include "thread-pool.h"
#include "qemu/main-loop.h"

typedef struct ThreadPoolElement ThreadPoolElement;

enum ThreadState {
    THREAD_QUEUED,
    THREAD_ACTIVE,
    THREAD_DONE,
};

#define MAX_THREADS 64

struct ThreadPoolElement {
    BlockAIOCB common;
    ThreadPool *pool;
    ThreadPoolFunc *func;
    void *arg;

    enum ThreadState state;
    int ret;

    /* Access to this list is protected by lock.  */
    QTAILQ_ENTRY(ThreadPoolElement) reqs;

    /* Access to this list is protected by the global mutex.  */
    QLIST_ENTRY(ThreadPoolElement) all;
};

struct ThreadPool {
    VeertuAioContext *ctx;
    QEMUBH *completion_bh;
    QemuMutex lock;
    QemuSemaphore sem;
    int max_threads;
    QemuThread threads[MAX_THREADS];

    /* The following variables are only accessed from one VeertuAioContext. */
    QLIST_HEAD(, ThreadPoolElement) head;

    /* The following variables are protected by lock.  */
    QTAILQ_HEAD(, ThreadPoolElement) request_list;

    bool stopping;
};

static void *worker_thread(void *opaque)
{
    ThreadPool *pool = opaque;

    while (!pool->stopping) {
        vmx_sem_timedwait(&pool->sem, 10000);

        vmx_mutex_lock(&pool->lock);
        if (QTAILQ_EMPTY(&pool->request_list)) {
            vmx_mutex_unlock(&pool->lock);
            continue;
        }

        ThreadPoolElement *req = QTAILQ_FIRST(&pool->request_list);
        QTAILQ_REMOVE(&pool->request_list, req, reqs);
        req->state = THREAD_ACTIVE;
        vmx_mutex_unlock(&pool->lock);

        req->ret = req->func(req->arg);
        smp_wmb();
        req->state = THREAD_DONE;

        vmx_mutex_lock(&pool->lock);
        vmx_bh_schedule(pool->completion_bh);
        vmx_mutex_unlock(&pool->lock);
    }

    return NULL;
}

static void spawn_thread(ThreadPool *pool, int i)
{
    vmx_thread_create(&pool->threads[i], "worker", worker_thread, pool, QEMU_THREAD_JOINABLE);
}

static void thread_pool_completion_bh(void *opaque)
{
    ThreadPool *pool = opaque;
    ThreadPoolElement *elem, *next;

restart:
    QLIST_FOREACH_SAFE(elem, &pool->head, all, next) {
        if (elem->state != THREAD_DONE)
            continue;

        if (elem->state == THREAD_DONE && elem->common.cb) {
            QLIST_REMOVE(elem, all);
            /* Read state before ret.  */
            smp_rmb();

            /* Schedule ourselves in case elem->common.cb() calls aio_poll() to
             * wait for another request that completed at the same time.
             */
            vmx_bh_schedule(pool->completion_bh);

            elem->common.cb(elem->common.opaque, elem->ret);
            vmx_aio_unref(elem);
            goto restart;
        } else {
            /* remove the request */
            QLIST_REMOVE(elem, all);
            vmx_aio_unref(elem);
        }
    }
}

static void thread_pool_cancel(BlockAIOCB *acb)
{
    ThreadPoolElement *elem = (ThreadPoolElement *)acb;
    ThreadPool *pool = elem->pool;

    vmx_mutex_lock(&pool->lock);
    if (elem->state == THREAD_QUEUED && vmx_sem_timedwait(&pool->sem, 0) == 0) {
        QTAILQ_REMOVE(&pool->request_list, elem, reqs);
        vmx_bh_schedule(pool->completion_bh);

        elem->state = THREAD_DONE;
        elem->ret = -ECANCELED;
    }

    vmx_mutex_unlock(&pool->lock);
}

static VeertuAioContext *thread_pool_get_aio_context(BlockAIOCB *acb)
{
    ThreadPoolElement *elem = (ThreadPoolElement *)acb;
    ThreadPool *pool = elem->pool;
    return pool->ctx;
}

static const AIOCBInfo thread_pool_aiocb_info = {
    .aiocb_size         = sizeof(ThreadPoolElement),
    .cancel_async       = thread_pool_cancel,
    .get_aio_context    = thread_pool_get_aio_context,
};

BlockAIOCB *thread_pool_submit_aio(ThreadPool *pool,
        ThreadPoolFunc *func, void *arg,
        BlockCompletionFunc *cb, void *opaque)
{
    ThreadPoolElement *req;

    req = vmx_aio_get(&thread_pool_aiocb_info, NULL, cb, opaque);
    req->func = func;
    req->arg = arg;
    req->state = THREAD_QUEUED;
    req->pool = pool;

    vmx_mutex_lock(&pool->lock);

    QLIST_INSERT_HEAD(&pool->head, req, all);
    QTAILQ_INSERT_TAIL(&pool->request_list, req, reqs);
    vmx_mutex_unlock(&pool->lock);

    vmx_sem_post(&pool->sem);
    return &req->common;
}

typedef struct ThreadPoolCo {
    Coroutine *co;
    int ret;
} ThreadPoolCo;

static void thread_pool_co_cb(void *opaque, int ret)
{
    ThreadPoolCo *co = opaque;

    co->ret = ret;
    vmx_coroutine_enter(co->co, NULL);
}

int coroutine_fn thread_pool_submit_co(ThreadPool *pool, ThreadPoolFunc *func,
                                       void *arg)
{
    ThreadPoolCo tpc = { .co = vmx_coroutine_self(), .ret = -EINPROGRESS };
    assert(vmx_in_coroutine());
    thread_pool_submit_aio(pool, func, arg, thread_pool_co_cb, &tpc);
    vmx_coroutine_yield();
    return tpc.ret;
}

void thread_pool_submit(ThreadPool *pool, ThreadPoolFunc *func, void *arg)
{
    thread_pool_submit_aio(pool, func, arg, NULL, NULL);
}

ThreadPool *thread_pool_create(VeertuAioContext *ctx, int thread_cnt)
{
    ThreadPool *pool = g_new(ThreadPool, 1);

    if (!ctx) {
        ctx = vmx_get_aio_context();
    }
    
    memset(pool, 0, sizeof(*pool));
    pool->ctx = ctx;
    pool->completion_bh = aio_bh_new(ctx, thread_pool_completion_bh, pool);
    vmx_mutex_init(&pool->lock);
    vmx_sem_init(&pool->sem, 0);
    pool->max_threads = thread_cnt;

    QLIST_INIT(&pool->head);
    QTAILQ_INIT(&pool->request_list);

    for (int i = 0; i < thread_cnt; i++)
        spawn_thread(pool, i);

    return pool;
}

void thread_pool_destroy(ThreadPool *pool)
{
    if (!pool)
        return;

    assert(QLIST_EMPTY(&pool->head));

    /* Wait for worker threads to terminate */
    pool->stopping = true;

    for (int i = 0; i < pool->max_threads; i++)
        vmx_sem_post(&pool->sem);
    for (int i = 0; i < pool->max_threads; i++)
        vmx_thread_join(&pool->threads[i]);

    vmx_bh_delete(pool->completion_bh);
    vmx_sem_destroy(&pool->sem);
    vmx_mutex_destroy(&pool->lock);
    g_free(pool);
}

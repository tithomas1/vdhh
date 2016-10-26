#ifndef QEMU_THREAD_POOL_H
#define QEMU_THREAD_POOL_H 1

#include "block.h"

typedef int ThreadPoolFunc(void *opaque);

typedef struct ThreadPool ThreadPool;

ThreadPool *thread_pool_create(struct VeertuAioContext *ctx, int threads);
void thread_pool_destroy(ThreadPool *pool);

BlockAIOCB *thread_pool_submit_aio(ThreadPool *pool,
        ThreadPoolFunc *func, void *arg,
        BlockCompletionFunc *cb, void *opaque);
int coroutine_fn thread_pool_submit_co(ThreadPool *pool,
        ThreadPoolFunc *func, void *arg);
void thread_pool_submit(ThreadPool *pool, ThreadPoolFunc *func, void *arg);

#endif

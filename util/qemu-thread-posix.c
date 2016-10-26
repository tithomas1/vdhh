#include <mach/mach_init.h>
#include <mach/task.h>
#include <mach/semaphore.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include "qemu/thread.h"
#include "qemu/atomic.h"
#include "qemu/notify.h"

void vmx_thread_naming(bool enable)
{
}

void vmx_mutex_init(QemuMutex *mutex)
{
    pthread_mutex_init(&mutex->lock, NULL);
}

void vmx_mutex_destroy(QemuMutex *mutex)
{
    pthread_mutex_destroy(&mutex->lock);
}

void vmx_mutex_lock(QemuMutex *mutex)
{
    pthread_mutex_lock(&mutex->lock);
}

int vmx_mutex_trylock(QemuMutex *mutex)
{
    return pthread_mutex_trylock(&mutex->lock);
}

void vmx_mutex_unlock(QemuMutex *mutex)
{
    pthread_mutex_unlock(&mutex->lock);
}

void vmx_cond_init(QemuCond *cond)
{
    pthread_cond_init(&cond->cond, NULL);
}

void vmx_cond_destroy(QemuCond *cond)
{
    pthread_cond_destroy(&cond->cond);
}

void vmx_cond_signal(QemuCond *cond)
{
    pthread_cond_signal(&cond->cond);
}

void vmx_cond_broadcast(QemuCond *cond)
{
    pthread_cond_broadcast(&cond->cond);
}

void vmx_cond_wait(QemuCond *cond, QemuMutex *mutex)
{
    pthread_cond_wait(&cond->cond, &mutex->lock);
}

void vmx_sem_init(QemuSemaphore *sem, int init)
{
    semaphore_create(mach_task_self(), &sem->sem, SYNC_POLICY_FIFO, init);
}

void vmx_sem_destroy(QemuSemaphore *sem)
{
    semaphore_destroy(mach_task_self(), sem->sem);
}

void vmx_sem_post(QemuSemaphore *sem)
{
     semaphore_signal(sem->sem);
}

int vmx_sem_timedwait(QemuSemaphore *sem, int ms)
{
    int retval = 0;
    mach_timespec_t mts;

    if (ms >= 0) {
        mts.tv_sec = ms / 1000;
        mts.tv_nsec = (ms % 1000) * 1000000;
    } else {
        mts.tv_sec = 0;
        mts.tv_nsec = 0;
    }
    retval = semaphore_timedwait(sem->sem, mts);
    switch (retval) {
        case KERN_SUCCESS:
            return 0;
        case KERN_OPERATION_TIMED_OUT:
            errno = ETIMEDOUT;
            break;
        case KERN_ABORTED:
            errno = EINTR;
            break;
        default:
            errno =  EINVAL;
            break;
    }
    return -1;
}

void vmx_sem_wait(QemuSemaphore *sem)
{
    semaphore_wait(sem->sem);
}

#define _EV_SET         0
#define _EV_FREE        1
#define _EV_BUSY       -1

void vmx_event_init(QemuEvent *ev, bool init)
{
    pthread_mutex_init(&ev->lock, NULL);
    pthread_cond_init(&ev->cond, NULL);

    ev->value = (init ? _EV_SET : _EV_FREE);
}

void vmx_event_destroy(QemuEvent *ev)
{
    pthread_mutex_destroy(&ev->lock);
    pthread_cond_destroy(&ev->cond);
}

void vmx_event_set(QemuEvent *ev)
{
    if (atomic_mb_read(&ev->value) != _EV_SET) {
        if (atomic_xchg(&ev->value, _EV_SET) == _EV_BUSY)
            pthread_cond_broadcast(&ev->cond);
    }
}

void vmx_event_reset(QemuEvent *ev)
{
    if (atomic_mb_read(&ev->value) == _EV_SET) {
        /*
         * If there was a concurrent reset (or even reset+wait),
         * do nothing.  Otherwise change EV_SET->EV_FREE.
         */
        atomic_or(&ev->value, _EV_FREE);
    }
}

void vmx_event_wait(QemuEvent *ev)
{
    unsigned value;

    value = atomic_mb_read(&ev->value);
    if (value != _EV_SET) {
        if (value == _EV_FREE) {
            if (atomic_cmpxchg(&ev->value, _EV_FREE, _EV_BUSY) == _EV_SET)
                return;
        }
        pthread_mutex_lock(&ev->lock);
        if (ev->value == value)
            pthread_cond_wait(&ev->cond, &ev->lock);
        pthread_mutex_unlock(&ev->lock);
    }
}

static pthread_key_t exit_key;

union NotifierThreadData {
    void *ptr;
    VeertuNotifiers list;
};

void vmx_thread_atexit_add(Notifier *notifier)
{
    union NotifierThreadData ntd;
    ntd.ptr = pthread_getspecific(exit_key);
    veertu_notifiers_add(&ntd.list, notifier);
    pthread_setspecific(exit_key, ntd.ptr);
}

void vmx_thread_atexit_remove(Notifier *notifier)
{
    union NotifierThreadData ntd;
    ntd.ptr = pthread_getspecific(exit_key);
    veertu_notifiers_remove(notifier);
    pthread_setspecific(exit_key, ntd.ptr);
}

static void vmx_thread_atexit_run(void *arg)
{
    union NotifierThreadData ntd = { .ptr = arg };
    veertu_notifiers_notify(&ntd.list, NULL);
}

static void __attribute__((constructor)) vmx_thread_atexit_init(void)
{
    pthread_key_create(&exit_key, vmx_thread_atexit_run);
}

void vmx_thread_create(QemuThread *thread, const char *name,
                       void *(*start_routine)(void*),
                       void *arg, int mode)
{
    sigset_t set, oldset;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    if (mode == QEMU_THREAD_DETACHED)
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    /* Leave signal handling to the iothread.  */
    sigfillset(&set);
    pthread_sigmask(SIG_SETMASK, &set, &oldset);
    pthread_create(&thread->thread, &attr, start_routine, arg);
    pthread_sigmask(SIG_SETMASK, &oldset, NULL);

    pthread_attr_destroy(&attr);
}

void vmx_thread_get_self(QemuThread *thread)
{
    thread->thread = pthread_self();
}

bool vmx_thread_is_self(QemuThread *thread)
{
   return pthread_equal(pthread_self(), thread->thread);
}

void vmx_thread_exit(void *retval)
{
    pthread_exit(retval);
}

void *vmx_thread_join(QemuThread *thread)
{
    void *ret;
    pthread_join(thread->thread, &ret);
    return ret;
}

#ifndef __QEMU_THREAD_H
#define __QEMU_THREAD_H 1

#include <inttypes.h>
#include <stdbool.h>

typedef struct QemuMutex QemuMutex;
typedef struct QemuCond QemuCond;
typedef struct QemuSemaphore QemuSemaphore;
typedef struct QemuEvent QemuEvent;
typedef struct QemuThread QemuThread;
typedef struct RFifoLock RFifoLock;

void rfifolock_init(RFifoLock *r, void (*cb)(void *), void *opaque);
void rfifolock_destroy(RFifoLock *r);
void rfifolock_lock(RFifoLock *r);
void rfifolock_unlock(RFifoLock *r);

#ifdef _WIN32
#include "qemu/thread-win32.h"
#else
#include "qemu/thread-posix.h"
#endif

#define QEMU_THREAD_JOINABLE 0
#define QEMU_THREAD_DETACHED 1

void vmx_mutex_init(QemuMutex *mutex);
void vmx_mutex_destroy(QemuMutex *mutex);
void vmx_mutex_lock(QemuMutex *mutex);
int vmx_mutex_trylock(QemuMutex *mutex);
void vmx_mutex_unlock(QemuMutex *mutex);

#define rcu_read_lock() do { } while (0)
#define rcu_read_unlock() do { } while (0)

void vmx_cond_init(QemuCond *cond);
void vmx_cond_destroy(QemuCond *cond);

/*
 * IMPORTANT: The implementation does not guarantee that pthread_cond_signal
 * and pthread_cond_broadcast can be called except while the same mutex is
 * held as in the corresponding pthread_cond_wait calls!
 */
void vmx_cond_signal(QemuCond *cond);
void vmx_cond_broadcast(QemuCond *cond);
void vmx_cond_wait(QemuCond *cond, QemuMutex *mutex);

void vmx_sem_init(QemuSemaphore *sem, int init);
void vmx_sem_post(QemuSemaphore *sem);
void vmx_sem_wait(QemuSemaphore *sem);
int vmx_sem_timedwait(QemuSemaphore *sem, int ms);
void vmx_sem_destroy(QemuSemaphore *sem);

void vmx_event_init(QemuEvent *ev, bool init);
void vmx_event_set(QemuEvent *ev);
void vmx_event_reset(QemuEvent *ev);
void vmx_event_wait(QemuEvent *ev);
void vmx_event_destroy(QemuEvent *ev);

void vmx_thread_create(QemuThread *thread, const char *name,
                        void *(*start_routine)(void *),
                        void *arg, int mode);
void *vmx_thread_join(QemuThread *thread);
void vmx_thread_get_self(QemuThread *thread);
bool vmx_thread_is_self(QemuThread *thread);
void vmx_thread_exit(void *retval);
void vmx_thread_naming(bool enable);

struct Notifier;
void vmx_thread_atexit_add(struct Notifier *notifier);
void vmx_thread_atexit_remove(struct Notifier *notifier);

#endif

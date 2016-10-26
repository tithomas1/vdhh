#include "qemu/compiler.h"
#include <stdint.h>

#ifndef __ATOMIC_H__
#define __ATOMIC_H__

#define barrier()   {__asm__ __volatile__("": : :"memory");}
#define smp_wmb()   barrier()
#define smp_rmb()   barrier()
#define smp_read_barrier_depends()   barrier()


#define smp_mb()    {__asm__ __volatile__("mfence" ::: "memory");}

#define atomic_fetch_inc(ptr)  __sync_fetch_and_add(ptr, 1)
#define atomic_fetch_dec(ptr)  __sync_fetch_and_add(ptr, -1)


#define atomic_inc(p)  __sync_fetch_and_add(p, 1)
#define atomic_dec(p)  __sync_fetch_and_add(p, -1)


#define atomic_or   __sync_fetch_and_or

#define atomic_read(p)              \
({                                  \
    (*(volatile typeof (*p) *)p);   \
})

#define atomic_mb_read(p)               \
({                                      \
    typeof(*p)  __v = atomic_read(p);   \
    smp_mb();                           \
    __v;                                \
})

#define atomic_cmpxchg __sync_val_compare_and_swap
#define atomic_xchg    __sync_swap


#endif

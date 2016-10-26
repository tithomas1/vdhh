/*
 * Bitmap Module
 *
 * Copyright (C) 2010 Corentin Chary <corentin.chary@gmail.com>
 *
 * Mostly inspired by (stolen from) linux/bitmap.h and linux/bitops.h
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING.LIB file in the top-level directory.
 */

#ifndef BITMAP_H
#define BITMAP_H

#include <glib.h>
#include <string.h>
#include <stdlib.h>

#include "qemu/osdep.h"
#include "qemu/bitops.h"


#define BITMAP_LAST_WORD_MASK(nbits)                                    \
    (                                                                   \
        ((nbits) % BITS_PER_LONG) ?                                     \
        (1UL<<((nbits) % BITS_PER_LONG))-1 : ~0UL                       \
        )

#define DECLARE_BITMAP(name,bits)                  \
        unsigned long name[BITS_TO_LONGS(bits)]

#define small_nbits(nbits)                      \
        ((nbits) <= BITS_PER_LONG)


static inline unsigned long *bitmap_try_new(long nbits)
{
    long len = BITS_TO_LONGS(nbits) * sizeof(unsigned long);
    return g_try_malloc0(len);
}

static inline unsigned long *bitmap_new(long nbits)
{
    unsigned long *ptr = bitmap_try_new(nbits);
    if (ptr == NULL) {
        abort();
    }
    
    return ptr;
}

static inline unsigned long *bitmap_new_dirty(long nbits)
{
    unsigned long *ptr = bitmap_try_new(nbits);
    if (ptr == NULL) {
        abort();
    }
    
    memset(ptr, 0xff, BITS_TO_LONGS(nbits) * sizeof(unsigned long));
    
    return ptr;
}

static inline void bitmap_zero(unsigned long *dst, long nbits)
{
    if (small_nbits(nbits)) {
        *dst = 0UL;
    } else {
        long len = BITS_TO_LONGS(nbits) * sizeof(unsigned long);
        memset(dst, 0, len);
    }
}



static inline unsigned long *bitmap_zero_extend(unsigned long *old,
                                                long old_nbits, long new_nbits)
{
    long new_len = BITS_TO_LONGS(new_nbits) * sizeof(unsigned long);
    unsigned long *new = g_realloc(old, new_len);
    memset(new, 0, new_len);
    //bitmap_clear(new, old_nbits, new_nbits - old_nbits);
    return new;
}

#endif /* BITMAP_H */

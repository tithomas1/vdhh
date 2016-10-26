/*
 * QEMU PC speaker emulation
 *
 * Copyright (c) 2006 Joachim Henke
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

#ifndef HW_PCSPK_H
#define HW_PCSPK_H

#include "hw.h"
#include "audio.h"
#include "audio/audio.h"
#include "qemu/timer.h"
#include "isa.h"

#define TYPE_PC_SPEAKER "isa-pcspk"

#define PCSPK_BUF_LEN 1792
#define PCSPK_SAMPLE_RATE 32000
#define PCSPK_MAX_FREQ (PCSPK_SAMPLE_RATE >> 1)
#define PCSPK_MIN_COUNT ((PIT_FREQ + PCSPK_MAX_FREQ - 1) / PCSPK_MAX_FREQ)

#define PC_SPEAKER(obj) obj

typedef struct {
    ISADevice parent;
    
    VeertuMemArea ioport;
    uint32_t iobase;
    uint8_t sample_buf[PCSPK_BUF_LEN];
    QEMUSoundCard card;
    SWVoiceOut *voice;
    void *pit;
    unsigned int pit_count;
    unsigned int samples;
    unsigned int play_pos;
    int data_on;
    int dummy_refresh_clock;
} PCSpkState;

static inline ISADevice *pcspk_init(ISABus *bus, ISADevice *pit)
{
    DeviceState *dev;
    ISADevice *isadev;
    PCSpkState *s;

    isadev = isa_create(bus, TYPE_PC_SPEAKER);
    dev = DEVICE(isadev);
    s = PC_SPEAKER(dev);
    
    s->iobase = 0x61;
    s->pit = pit;

    qdev_init_nofail(dev);

    return isadev;
}

#endif /* !HW_PCSPK_H */

/*
 * QEMU VMPort emulation
 *
 * Copyright (C) 2007 Hervé Poussineau
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
#include "hw.h"
#include "isa.h"
#include "ipc.h"
#include "veertuemu.h"
#include "nqdev.h"
#include "vmm/vmx.h"

//#define VMPORT_DEBUG

#define VMPORT_CMD_GETVERSION 0x0a
#define VMPORT_CMD_GETRAMSIZE 0x14

#define VMPORT_ENTRIES 0x2c
#define VMPORT_MAGIC   0x564D5868

#define TYPE_VMPORT "vmport"
#define VMPORT(obj) obj

typedef struct VMPortState
{
    ISADevice parent;

    VeertuMemArea io;
    VMPortReadFunc *func[VMPORT_ENTRIES];
    void *opaque[VMPORT_ENTRIES];
} VMPortState;

static VMPortState *port_state;

void vmport_register(unsigned char command, VMPortReadFunc *func, void *opaque)
{
    if (command >= VMPORT_ENTRIES)
        return;

    port_state->func[command] = func;
    port_state->opaque[command] = opaque;
}

static uint64_t vmport_ioport_read(void *opaque, hwaddr addr,
                                   unsigned size)
{
    VMPortState *s = opaque;
    CPUState *cs = current_cpu;
    X86CPU *cpu = X86_CPU(cs);
    CPUX86State *env = &cpu->env;
    unsigned char command;
    uint32_t eax;


    eax = EAX(cs);
    if (eax != VMPORT_MAGIC)
        return eax;

    command = ECX(cs);
    if (command >= VMPORT_ENTRIES)
        return eax;
    if (!s->func[command])
    {
#ifdef VMPORT_DEBUG
        fprintf(stderr, "vmport: unknown command %x\n", command);
#endif
        return eax;
    }

    return s->func[command](s->opaque[command], addr);
}

static void vmport_ioport_write(void *opaque, hwaddr addr,
                                uint64_t val, unsigned size)
{
    CPUState *cs = current_cpu;
    X86CPU *cpu = X86_CPU(current_cpu);

    EAX(cs) = vmport_ioport_read(opaque, addr, 4);
}

static uint32_t vmport_cmd_get_version(void *opaque, uint32_t addr)
{
    CPUState *cs = current_cpu;
    X86CPU *cpu = X86_CPU(current_cpu);

    EBX(cs) = VMPORT_MAGIC;
    return 6;
}

static uint32_t vmport_cmd_ram_size(void *opaque, uint32_t addr)
{
    CPUState *cs = current_cpu;
    X86CPU *cpu = X86_CPU(current_cpu);

    EBX(cs) = 0x1177;
    return ram_size;
}

/* vmmouse helpers */
void vmmouse_get_data(uint32_t *data)
{
    CPUState *cs = current_cpu;
    X86CPU *cpu = X86_CPU(current_cpu);
    CPUX86State *env = &cpu->env;

    data[0] = EAX(cs); data[1] = EBX(cs);
    data[2] = ECX(cs); data[3] = EDX(cs);
    data[4] = ESI(cs); data[5] = EDI(cs);
}

void vmmouse_set_data(const uint32_t *data)
{
    CPUState *cs = current_cpu;
    X86CPU *cpu = X86_CPU(current_cpu);
    CPUX86State *env = &cpu->env;

    EAX(cs) = data[0]; EBX(cs) = data[1];
    ECX(cs) = data[2]; EDX(cs) = data[3];
    ESI(cs) = data[4]; EDI(cs) = data[5];
}

static const MemAreaOps vmport_ops = {
    .read = vmport_ioport_read,
    .write = vmport_ioport_write,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void vmport_realizefn(DeviceState *dev, Error **errp)
{
    ISADevice *isadev = ISA_DEVICE(dev);
    VMPortState *s = VMPORT(dev);

    memory_area_init_io(&s->io, VeertuTypeHold(s), &vmport_ops, s, "vmport", 1);
    isa_register_ioport(isadev, &s->io, 0x5658);

    port_state = s;
    /* Register some generic port commands */
    vmport_register(VMPORT_CMD_GETVERSION, vmport_cmd_get_version, NULL);
    vmport_register(VMPORT_CMD_GETRAMSIZE, vmport_cmd_ram_size, NULL);
}

static void vmport_class_initfn(VeertuTypeClassHold *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = vmport_realizefn;
    /* Reason: realize sets global port_state */
    dc->cannot_instantiate_with_device_add_yet = true;
}

static const VeertuTypeInfo vmport_info = {
    .name          = TYPE_VMPORT,
    .parent        = TYPE_ISA_DEVICE,
    .instance_size = sizeof(VMPortState),
    .class_init    = vmport_class_initfn,
};

void vmport_register_types(void)
{
    register_type_internal(&vmport_info);
}

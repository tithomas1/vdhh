/*
 * QEMU PCI VGA Emulator.
 *
 * see docs/specs/standard-vga.txt for virtual hardware specs.
 *
 * Copyright (c) 2003 Fabrice Bellard
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
#include "ui/console.h"
#include "pci.h"
#include "vga_int.h"
#include "ui/pixel_ops.h"
#include "qemu/timer.h"
#include "loader.h"

#define PCI_VGA_IOPORT_OFFSET 0x400
#define PCI_VGA_IOPORT_SIZE   (0x3e0 - 0x3c0)
#define PCI_VGA_BOCHS_OFFSET  0x500
#define PCI_VGA_BOCHS_SIZE    (0x0b * 2)
#define PCI_VGA_QEXT_OFFSET   0x600
#define PCI_VGA_QEXT_SIZE     (2 * 4)
#define PCI_VGA_MMIO_SIZE     0x1000

#define PCI_VGA_QEXT_REG_SIZE         (0 * 4)
#define PCI_VGA_QEXT_REG_BYTEORDER    (1 * 4)
#define  PCI_VGA_QEXT_LITTLE_ENDIAN   0x1e1e1e1e
#define  PCI_VGA_QEXT_BIG_ENDIAN      0xbebebebe

enum vga_pci_flags {
    PCI_VGA_FLAG_ENABLE_MMIO = 1,
    PCI_VGA_FLAG_ENABLE_QEXT = 2,
};

typedef struct PCIVGAState {
    PCIDevice dev;
    VGACommonState vga;
    uint32_t flags;
    VeertuMemArea mmio;
    VeertuMemArea ioport;
    VeertuMemArea bochs;
    VeertuMemArea qext;
} PCIVGAState;

static const VMStateDescription vmstate_vga_pci = {
    .name = "vga",
    .version_id = 2,
    .minimum_version_id = 2,
    .fields = (VMStateField[]) {
        VMSTATE_PCI_DEVICE(dev, PCIVGAState),
        VMSTATE_STRUCT(vga, PCIVGAState, 0, vmstate_vga_common, VGACommonState),
        VMSTATE_END_OF_LIST()
    }
};

static uint64_t pci_vga_ioport_read(void *ptr, hwaddr addr,
                                    unsigned size)
{
    PCIVGAState *d = ptr;
    uint64_t ret = 0;

    switch (size) {
    case 1:
        ret = vga_ioport_read(&d->vga, addr);
        break;
    case 2:
        ret  = vga_ioport_read(&d->vga, addr);
        ret |= vga_ioport_read(&d->vga, addr+1) << 8;
        break;
    }
    return ret;
}

static void pci_vga_ioport_write(void *ptr, hwaddr addr,
                                 uint64_t val, unsigned size)
{
    PCIVGAState *d = ptr;

    switch (size) {
    case 1:
        vga_ioport_write(&d->vga, addr + 0x3c0, val);
        break;
    case 2:
        /*
         * Update bytes in little endian order.  Allows to update
         * indexed registers with a single word write because the
         * index byte is updated first.
         */
        vga_ioport_write(&d->vga, addr + 0x3c0, val & 0xff);
        vga_ioport_write(&d->vga, addr + 0x3c1, (val >> 8) & 0xff);
        break;
    }
}

static const MemAreaOps pci_vga_ioport_ops = {
    .read = pci_vga_ioport_read,
    .write = pci_vga_ioport_write,
    .valid.min_access_size = 1,
    .valid.max_access_size = 4,
    .impl.min_access_size = 1,
    .impl.max_access_size = 2,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static uint64_t pci_vga_bochs_read(void *ptr, hwaddr addr,
                                   unsigned size)
{
    PCIVGAState *d = ptr;
    int index = addr >> 1;

    vbe_ioport_write_index(&d->vga, 0, index);
    return vbe_ioport_read_data(&d->vga, 0);
}

static void pci_vga_bochs_write(void *ptr, hwaddr addr,
                                uint64_t val, unsigned size)
{
    PCIVGAState *d = ptr;
    int index = addr >> 1;

    vbe_ioport_write_index(&d->vga, 0, index);
    vbe_ioport_write_data(&d->vga, 0, val);
}

static const MemAreaOps pci_vga_bochs_ops = {
    .read = pci_vga_bochs_read,
    .write = pci_vga_bochs_write,
    .valid.min_access_size = 1,
    .valid.max_access_size = 4,
    .impl.min_access_size = 2,
    .impl.max_access_size = 2,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static uint64_t pci_vga_qext_read(void *ptr, hwaddr addr, unsigned size)
{
    PCIVGAState *d = ptr;

    switch (addr) {
    case PCI_VGA_QEXT_REG_SIZE:
        return PCI_VGA_QEXT_SIZE;
    case PCI_VGA_QEXT_REG_BYTEORDER:
        return d->vga.big_endian_fb ?
            PCI_VGA_QEXT_BIG_ENDIAN : PCI_VGA_QEXT_LITTLE_ENDIAN;
    default:
        return 0;
    }
}

static void pci_vga_qext_write(void *ptr, hwaddr addr,
                               uint64_t val, unsigned size)
{
    PCIVGAState *d = ptr;

    switch (addr) {
    case PCI_VGA_QEXT_REG_BYTEORDER:
        if (val == PCI_VGA_QEXT_BIG_ENDIAN) {
            d->vga.big_endian_fb = true;
        }
        if (val == PCI_VGA_QEXT_LITTLE_ENDIAN) {
            d->vga.big_endian_fb = false;
        }
        break;
    }
}

static const MemAreaOps pci_vga_qext_ops = {
    .read = pci_vga_qext_read,
    .write = pci_vga_qext_write,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static int pci_std_vga_initfn(PCIDevice *dev)
{
    PCIVGAState *d = DO_UPCAST(PCIVGAState, dev, dev);
    VGACommonState *s = &d->vga;

    /* vga + console init */
    vga_common_init(s, VeertuTypeHold(dev), true);
    vga_init(s, VeertuTypeHold(dev), pci_address_space(dev), pci_address_space_io(dev),
             true);

    s->con = graphic_console_init(DEVICE(dev), 0, s->hw_ops, s);

    /* XXX: VGA_RAM_SIZE must be a power of two */
    pci_register_bar(&d->dev, 0, PCI_BASE_ADDRESS_MEM_PREFETCH, &s->vram);

    /* mmio bar for vga register access */
    if (d->flags & (1 << PCI_VGA_FLAG_ENABLE_MMIO)) {
        memory_area_init(&d->mmio,  "vga.mmio", 4096);
        memory_area_init_io(&d->ioport, NULL, &pci_vga_ioport_ops, d,
                              "vga ioports remapped", PCI_VGA_IOPORT_SIZE);
        memory_area_init_io(&d->bochs, NULL, &pci_vga_bochs_ops, d,
                              "bochs dispi interface", PCI_VGA_BOCHS_SIZE);

        mem_area_add_child(&d->mmio, PCI_VGA_IOPORT_OFFSET,
                                    &d->ioport);
        mem_area_add_child(&d->mmio, PCI_VGA_BOCHS_OFFSET,
                                    &d->bochs);

        if (d->flags & (1 << PCI_VGA_FLAG_ENABLE_QEXT)) {
            memory_area_init_io(&d->qext, NULL, &pci_vga_qext_ops, d,
                                  "vmx extended regs", PCI_VGA_QEXT_SIZE);
            mem_area_add_child(&d->mmio, PCI_VGA_QEXT_OFFSET,
                                        &d->qext);
            pci_set_byte(&d->dev.config[PCI_REVISION_ID], 2);
        }

        pci_register_bar(&d->dev, 2, PCI_BASE_ADDRESS_SPACE_MEMORY, &d->mmio);
    }

    if (!dev->rom_bar) {
        /* compatibility with pc-0.13 and older */
        vga_init_vbe(s, VeertuTypeHold(dev), pci_address_space(dev));
    }

    return 0;
}

static int pci_secondary_vga_initfn(PCIDevice *dev)
{
    PCIVGAState *d = DO_UPCAST(PCIVGAState, dev, dev);
    VGACommonState *s = &d->vga;

    /* vga + console init */
    vga_common_init(s, VeertuTypeHold(dev), false);
    s->con = graphic_console_init(DEVICE(dev), 0, s->hw_ops, s);

    /* mmio bar */
    memory_area_init(&d->mmio,  "vga.mmio", 4096);
    memory_area_init_io(&d->ioport, VeertuTypeHold(dev), &pci_vga_ioport_ops, d,
                          "vga ioports remapped", PCI_VGA_IOPORT_SIZE);
    memory_area_init_io(&d->bochs, VeertuTypeHold(dev), &pci_vga_bochs_ops, d,
                          "bochs dispi interface", PCI_VGA_BOCHS_SIZE);

    mem_area_add_child(&d->mmio, PCI_VGA_IOPORT_OFFSET,
                                &d->ioport);
    mem_area_add_child(&d->mmio, PCI_VGA_BOCHS_OFFSET,
                                &d->bochs);

    if (d->flags & (1 << PCI_VGA_FLAG_ENABLE_QEXT)) {
        memory_area_init_io(&d->qext, NULL, &pci_vga_qext_ops, d,
                              "vmx extended regs", PCI_VGA_QEXT_SIZE);
        mem_area_add_child(&d->mmio, PCI_VGA_QEXT_OFFSET,
                                    &d->qext);
        pci_set_byte(&d->dev.config[PCI_REVISION_ID], 2);
    }

    pci_register_bar(&d->dev, 0, PCI_BASE_ADDRESS_MEM_PREFETCH, &s->vram);
    pci_register_bar(&d->dev, 2, PCI_BASE_ADDRESS_SPACE_MEMORY, &d->mmio);

    return 0;
}

static void pci_secondary_vga_reset(DeviceState *dev)
{
    PCIVGAState *d = DO_UPCAST(PCIVGAState, dev.qdev, dev);

    vga_common_reset(&d->vga);
}



static void vga_class_init(VeertuTypeClassHold *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->init = pci_std_vga_initfn;
    k->romfile = "vgabios-stdvga.bin";
    k->vendor_id = PCI_VENDOR_ID_QEMU;
    k->device_id = PCI_DEVICE_ID_QEMU_VGA;
    k->class_id = PCI_CLASS_DISPLAY_VGA;
    dc->vmsd = &vmstate_vga_pci;
    dc->hotpluggable = false;
    set_bit(DEVICE_CATEGORY_DISPLAY, dc->categories);
}

static void secondary_class_init(VeertuTypeClassHold *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->init = pci_secondary_vga_initfn;
    k->vendor_id = PCI_VENDOR_ID_QEMU;
    k->device_id = PCI_DEVICE_ID_QEMU_VGA;
    k->class_id = PCI_CLASS_DISPLAY_OTHER;
    dc->vmsd = &vmstate_vga_pci;
    dc->reset = pci_secondary_vga_reset;
    set_bit(DEVICE_CATEGORY_DISPLAY, dc->categories);
}

static const VeertuTypeInfo vga_info = {
    .name          = "VGA",
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(PCIVGAState),
    .class_init    = vga_class_init,
};

static const VeertuTypeInfo secondary_info = {
    .name          = "secondary-vga",
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(PCIVGAState),
    .class_init    = secondary_class_init,
};

void vga_register_types(void)
{
    register_type_internal(&vga_info);
    register_type_internal(&secondary_info);
}

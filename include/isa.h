#ifndef HW_ISA_H
#define HW_ISA_H

/* ISA bus */

#include "ioport.h"
#include "memory.h"
#include "nqdev.h"

#define ISA_NUM_IRQS 16

#define TYPE_ISA_DEVICE "isa-device"
#define ISA_DEVICE(obj) obj
#define ISA_DEVICE_CLASS(klass) klass
#define ISA_DEVICE_GET_CLASS(obj) ((VeertuType *)obj)->class

#define TYPE_ISA_BUS "ISA"
#define ISA_BUS(obj) obj

#define TYPE_APPLE_SMC "isa-applesmc"

static inline bool applesmc_find(void)
{
    return NULL;
}

typedef struct ISADeviceClass {
    DeviceClass parent_class;
} ISADeviceClass;

struct ISABus {
    /*< private >*/
    BusState parent;
    /*< public >*/

    VeertuMemArea *address_space_io;
    vmx_irq *irqs;
};

struct ISADevice {
    /*< private >*/
    DeviceState parent;
    /*< public >*/

    uint32_t isairq[2];
    int nirqs;
    int ioport_id;
};

ISABus *isa_bus_new(DeviceState *dev, VeertuMemArea *address_space_io);
void isa_bus_irqs(ISABus *bus, vmx_irq *irqs);
vmx_irq isa_get_irq(ISADevice *dev, int isairq);
void isa_init_irq(ISADevice *dev, vmx_irq *p, int isairq);
VeertuMemArea *isa_address_space(ISADevice *dev);
VeertuMemArea *isa_address_space_io(ISADevice *dev);
ISADevice *isa_create(ISABus *bus, const char *name);
ISADevice *isa_try_create(ISABus *bus, const char *name);
ISADevice *isa_create_simple(ISABus *bus, const char *name);

ISADevice *isa_vga_init(ISABus *bus);

/**
 * isa_register_ioport: Install an I/O port region on the ISA bus.
 *
 * Register an I/O port region via mem_area_add_child
 * inside the ISA I/O address space.
 *
 * @dev: the ISADevice against which these are registered; may be NULL.
 * @io: the #VeertuMemArea being registered.
 * @start: the base I/O port.
 */
void isa_register_ioport(ISADevice *dev, VeertuMemArea *io, uint16_t start);

/**
 * isa_register_portio_list: Initialize a set of ISA io ports
 *
 * Several ISA devices have many dis-joint I/O ports.  Worse, these I/O
 * ports can be interleaved with I/O ports from other devices.  This
 * function makes it easy to create multiple MemoryRegions for a single
 * device and use the legacy portio routines.
 *
 * @dev: the ISADevice against which these are registered; may be NULL.
 * @start: the base I/O port against which the portio->offset is applied.
 * @portio: the ports, sorted by offset.
 * @opaque: passed into the portio callbacks.
 * @name: passed into memory_area_init_io.
 */
void isa_register_portio_list(ISADevice *dev, uint16_t start,
                              const MemoryRegionPortio *portio,
                              void *opaque, const char *name);

static inline ISABus *isa_bus_from_device(ISADevice *d)
{
    return ISA_BUS(qdev_get_parent_bus(DEVICE(d)));
}

extern hwaddr isa_mem_base;

/* dma.c */
int DMA_get_channel_mode (int nchan);
int DMA_read_memory (int nchan, void *buf, int pos, int size);
int DMA_write_memory (int nchan, void *buf, int pos, int size);
void DMA_hold_DREQ (int nchan);
void DMA_release_DREQ (int nchan);
void DMA_schedule(int nchan);
void DMA_init(int high_page_enable, vmx_irq *cpu_request_exit);
void DMA_register_channel (int nchan,
                           DMA_transfer_handler transfer_handler,
                           void *opaque);
#endif

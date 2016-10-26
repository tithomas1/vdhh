/*
 *  Platform Bus device to support dynamic Sysbus devices
 *
 * Copyright (C) 2014 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Author: Alexander Graf, <agraf@suse.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "platform-bus.h"
#include "monitor/monitor.h"
#include "address-spaces.h"
#include "sysemu.h"

/*
 * Returns the PlatformBus MMIO region offset for Region n of a SysBusDevice or
 * -1 if the region is not mapped on this Platform bus.
 */
hwaddr platform_bus_get_mmio_addr(PlatformBusDevice *pbus, SysBusDevice *sbdev,
                                  int n)
{
    return -1;
}

static int platform_bus_count_irqs(SysBusDevice *sbdev, void *opaque)
{
    return 0;
}

/*
 * Loop through all sysbus devices and look for unassigned IRQ lines as well as
 * unassociated MMIO regions. Connect them to the platform bus if available.
 */
static void plaform_bus_refresh_irqs(PlatformBusDevice *pbus)
{
    bitmap_zero(pbus->used_irqs, pbus->num_irqs);
    foreach_dynamic_sysbus_device(platform_bus_count_irqs, pbus);
    pbus->done_gathering = true;
}

static int platform_bus_map_irq(PlatformBusDevice *pbus, SysBusDevice *sbdev,
                                int n)
{
    int max_irqs = pbus->num_irqs;
    int irqn;

    if (sysbus_is_irq_connected(sbdev, n)) {
        /* IRQ is already mapped, nothing to do */
        return 0;
    }

    irqn = find_first_zero_bit(pbus->used_irqs, max_irqs);
    if (irqn >= max_irqs) {
        hw_error("Platform Bus: Can not fit IRQ line");
        return -1;
    }

    set_bit(irqn, pbus->used_irqs);

    return 0;
}

static int platform_bus_map_mmio(PlatformBusDevice *pbus, SysBusDevice *sbdev,
                                 int n)
{
    VeertuMemArea *sbdev_mr = sysbus_mmio_get_region(sbdev, n);
    uint64_t size = mem_area_get_size(sbdev_mr);
    uint64_t alignment = (1ULL << (63 - clz64(size + size - 1)));
    uint64_t off;
    bool found_region = false;
#if 0
    if (memory_region_is_mapped(sbdev_mr)) {
        /* Region is already mapped, nothing to do */
        return 0;
    }
#endif
    /*
     * Look for empty space in the MMIO space that is naturally aligned with
     * the target device's memory region
     */
    for (off = 0; off < pbus->mmio_size; off += alignment) {
        if (!memory_area_find(&pbus->mmio, off, size).mr) {
            found_region = true;
            break;
        }
    }

    if (!found_region) {
        hw_error("Platform Bus: Can not fit MMIO region of size %"PRIx64, size);
    }

    /* Map the device's region into our Platform Bus MMIO space */
    mem_area_add_child(&pbus->mmio, off, sbdev_mr);

    return 0;
}

/*
 * For each sysbus device, look for unassigned IRQ lines as well as
 * unassociated MMIO regions. Connect them to the platform bus if available.
 */
static int link_sysbus_device(SysBusDevice *sbdev, void *opaque)
{
    PlatformBusDevice *pbus = opaque;
    int i;

    for (i = 0; sysbus_has_irq(sbdev, i); i++) {
        platform_bus_map_irq(pbus, sbdev, i);
    }

    for (i = 0; sysbus_has_mmio(sbdev, i); i++) {
        platform_bus_map_mmio(pbus, sbdev, i);
    }

    return 0;
}

static void platform_bus_init_notify(Notifier *notifier, void *data)
{
    PlatformBusDevice *pb = container_of(notifier, PlatformBusDevice, notifier);

    /*
     * Generate a bitmap of used IRQ lines, as the user might have specified
     * them on the command line.
     */
    plaform_bus_refresh_irqs(pb);

    foreach_dynamic_sysbus_device(link_sysbus_device, pb);
}

static void platform_bus_realize(DeviceState *dev, Error **errp)
{
    PlatformBusDevice *pbus;
    SysBusDevice *d;
    int i;

    d = SYS_BUS_DEVICE(dev);
    pbus = PLATFORM_BUS_DEVICE(dev);

    memory_area_init(&pbus->mmio,"platform bus", pbus->mmio_size);
    sysbus_init_mmio(d, &pbus->mmio);

    pbus->used_irqs = bitmap_new(pbus->num_irqs);
    pbus->irqs = g_new0(vmx_irq, pbus->num_irqs);
    for (i = 0; i < pbus->num_irqs; i++) {
        sysbus_init_irq(d, &pbus->irqs[i]);
    }

    /*
     * Register notifier that allows us to gather dangling devices once the
     * machine is completely assembled
     */
    pbus->notifier.notify = platform_bus_init_notify;
    vmx_add_machine_init_done_notifier(&pbus->notifier);
}

static void platform_bus_class_init(VeertuTypeClassHold *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = platform_bus_realize;
}

static const VeertuTypeInfo platform_bus_info = {
    .name          = TYPE_PLATFORM_BUS_DEVICE,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(PlatformBusDevice),
    .class_init    = platform_bus_class_init,
};

void platform_bus_register_types(void)
{
    register_type_internal(&platform_bus_info);
}


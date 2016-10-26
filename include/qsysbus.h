#ifndef HW_SYSBUS_H
#define HW_SYSBUS_H 1

/* Devices attached directly to the main system bus.  */

#include "nqdev.h"
#include "memory.h"

#define QDEV_MAX_MMIO 32
#define QDEV_MAX_PIO 32

#define TYPE_SYSTEM_BUS "System"
#define SYSTEM_BUS(obj) obj

typedef struct SysBusDevice SysBusDevice;

#define TYPE_SYS_BUS_DEVICE "sys_busdev"
#define SYS_BUS_DEVICE(obj) obj
#define SYS_BUS_DEVICE_CLASS(klass) klass
#define SYS_BUS_DEVICE_GET_CLASS(obj) ((VeertuType *)obj)->class

/**
 * SysBusDeviceClass:
 * @init: Callback function invoked when the #DeviceState.realized property
 * is changed to %true. Deprecated, new types inheriting directly from
 * TYPE_SYS_BUS_DEVICE should use #DeviceClass.realize instead, new leaf
 * types should consult their respective parent type.
 *
 * SysBusDeviceClass is not overriding #DeviceClass.realize, so derived
 * classes overriding it are not required to invoke its implementation.
 */

#define SYSBUS_DEVICE_GPIO_IRQ "sysbus-irq"

typedef struct SysBusDeviceClass {
    /*< private >*/
    DeviceClass parent_class;
    /*< public >*/

    int (*init)(SysBusDevice *dev);
} SysBusDeviceClass;

struct SysBusDevice {
    /*< private >*/
    DeviceState parent;
    /*< public >*/

    int num_mmio;
    struct {
        hwaddr addr;
        VeertuMemArea *memory;
    } mmio[QDEV_MAX_MMIO];
    int num_pio;
    pio_addr_t pio[QDEV_MAX_PIO];
};

typedef int FindSysbusDeviceFunc(SysBusDevice *sbdev, void *opaque);

void sysbus_init_mmio(SysBusDevice *dev, VeertuMemArea *memory);
VeertuMemArea *sysbus_mmio_get_region(SysBusDevice *dev, int n);
void sysbus_init_irq(SysBusDevice *dev, vmx_irq *p);
void sysbus_pass_irq(SysBusDevice *dev, SysBusDevice *target);
void sysbus_init_ioports(SysBusDevice *dev, pio_addr_t ioport, pio_addr_t size);


bool sysbus_has_irq(SysBusDevice *dev, int n);
bool sysbus_has_mmio(SysBusDevice *dev, unsigned int n);
bool sysbus_is_irq_connected(SysBusDevice *dev, int n);
void sysbus_mmio_map(SysBusDevice *dev, int n, hwaddr addr);
void sysbus_mmio_map_overlap(SysBusDevice *dev, int n, hwaddr addr,
                             int priority);
void sysbus_add_io(SysBusDevice *dev, hwaddr addr,
                   VeertuMemArea *mem);
VeertuMemArea *sysbus_address_space(SysBusDevice *dev);

/* Call func for every dynamically created sysbus device in the system */
void foreach_dynamic_sysbus_device(FindSysbusDeviceFunc *func, void *opaque);

/* Legacy helper function for creating devices.  */
DeviceState *sysbus_create_varargs(const char *name,
                                 hwaddr addr, ...);
DeviceState *sysbus_try_create_varargs(const char *name,
                                       hwaddr addr, ...);
static inline DeviceState *sysbus_create_simple(const char *name,
                                              hwaddr addr,
                                              vmx_irq irq)
{
    return sysbus_create_varargs(name, addr, irq, NULL);
}

static inline DeviceState *sysbus_try_create_simple(const char *name,
                                                    hwaddr addr,
                                                    vmx_irq irq)
{
    return sysbus_try_create_varargs(name, addr, irq, NULL);
}

#endif /* !HW_SYSBUS_H */

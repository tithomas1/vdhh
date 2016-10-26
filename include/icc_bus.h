/* icc_bus.h
 * emulate x86 ICC (Interrupt Controller Communications) bus
 *
 * Copyright (c) 2013 Red Hat, Inc
 *
 * Authors:
 *     Igor Mammedov <imammedo@redhat.com>
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>
 */
#ifndef ICC_BUS_H
#define ICC_BUS_H

#include "memory.h"
#include "qdev-core.h"

#define TYPE_ICC_BUS "icc-bus"

#ifndef CONFIG_USER_ONLY

/**
 * ICCBus:
 *
 * ICC bus
 */
typedef struct ICCBus {
    /*< private >*/
    BusState parent;
    /*< public >*/

    VeertuMemArea *apic_address_space;
} ICCBus;

#define ICC_BUS(obj) obj

/**
 * ICCDevice:
 *
 * ICC device
 */
typedef struct ICCDevice {
    /*< private >*/
    DeviceState qdev;
    /*< public >*/
} ICCDevice;

/**
 * ICCDeviceClass:
 * @init: Initialization callback for derived classes.
 *
 * ICC device class
 */
typedef struct ICCDeviceClass {
    /*< private >*/
    DeviceClass parent_class;
    /*< public >*/

    DeviceRealize realize;
} ICCDeviceClass;

#define TYPE_ICC_DEVICE "icc-device"
#define ICC_DEVICE(obj) obj
#define ICC_DEVICE_CLASS(klass) klass
#define ICC_DEVICE_GET_CLASS(obj) ((VeertuType *)obj)->class

#define TYPE_ICC_BRIDGE "icc-bridge"

#endif /* CONFIG_USER_ONLY */
#endif

/*
 * QEMU Common PCI Host bridge configuration data space access routines.
 *
 * Copyright (c) 2006 Fabrice Bellard
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

/* Worker routines for a PCI host controller that uses an {address,data}
   register pair to access PCI configuration space.  */

#ifndef PCI_HOST_H
#define PCI_HOST_H

#include "qsysbus.h"

#define TYPE_PCI_HOST_BRIDGE "host_pci_bridge"
#define PCI_HOST_BRIDGE(obj) obj
#define PCI_HOST_BRIDGE_CLASS(klass) klass
#define PCI_HOST_BRIDGE_GET_CLASS(obj) ((VeertuType *)obj)->class

struct PCIHostState {
    SysBusDevice busdev;

    VeertuMemArea conf_mem;
    VeertuMemArea data_mem;
    VeertuMemArea mmcfg;
    uint32_t config_reg;
    PCIBus *bus;

    QLIST_ENTRY(PCIHostState) next;
};

typedef struct PCIHostBridgeClass {
    SysBusDeviceClass parent_class;

    const char *(*root_bus_path)(PCIHostState *, PCIBus *);
} PCIHostBridgeClass;

/* common internal helpers for PCI/PCIe hosts, cut off overflows */

#endif /* PCI_HOST_H */

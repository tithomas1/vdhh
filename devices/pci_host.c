#include "pci_host.h"

VeertuTypeInfo veertu_pci_type_info = {
    .name = TYPE_PCI_HOST_BRIDGE,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(PCIHostState),
    .class_size = sizeof(PCIHostBridgeClass),
};


void veertu_pci_register_types()
{
    register_type_internal(&veertu_pci_type_info);
}

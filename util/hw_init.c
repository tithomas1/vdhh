/*
 * Copyright (C) 2016 Veertu Inc,
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qsysbus.h"

// call this function to trigger statis initializers (libraries)
void hw_init() {
}

void icc_bus_register_types();
void register_accel_types(void);
void sysbus_ahci_register_types(void);
void apic_register_types(void);
void apic_common_register_types(void);
void usb_register_types(void);
void register_types(void);
void i2c_slave_register_types(void);
void cpu_register_types(void);
void x86_cpu_register_types(void);
void usb_audio_register_types(void);
void usb_hub_register_types(void);
void usb_msd_register_types(void);
void e1000_register_types(void);
//void fw_path_provider_register_types(void);
void fw_cfg_register_types(void);
void ehci_pci_register_types(void);
void ehci_sysbus_register_types(void);
void ohci_register_types(void);
void uhci_register_types(void);
void xhci_register_types(void);
void usb_host_register_types(void);
void usb_host_register_legacy_types(void);
void pit_register_types(void);
void register_devices(void);
void pic_register_types(void);
void pic_common_register_types(void);
void d2pbr_register(void);
void ich_ahci_register_types(void);
void ioapic_register_types(void);
void ioapic_common_register_types(void);
void irq_register_types(void);
void isabus_register_types(void);
void isa_ide_register_types(void);
void ich9_lpc_register(void);
void lsi53c895a_register_types(void);
void veertu_machine_register();
void mc146818rtc_register_types(void);
void megasas_register_types(void);
void pc_machine_register_types(void);
void port92_register_types(void);
void pci_ide_register_types(void);
void pci_register_types(void);
void pci_bridge_register_types(void);
void pci_bridge_dev_register(void);
void veertu_pci_register_types(void);
void i8042_register_types(void);
void pcspk_register(void);
void i440fx_register_types(void);
void piix_ide_register_types(void);
void piix4_pm_register_types(void);
void platform_bus_register_types(void);
void ide_register_types(void);
void qdev_register_types(void);
void rtl8139_register_types(void);
void scsi_register_types(void);
void scsi_disk_register_types(void);
void scsi_generic_register_types(void);
//void serial_register_types(void);
//void serial_pci_register_types(void);
void smbus_device_register_types(void);
void smbus_eeprom_register_types(void);
void ich9_smb_register(void);
void sysbus_register_types(void);
void vga_register_types(void);
void vmmouse_register_types(void);
void vmport_register_types(void);
void vmsvga_register_types(void);
void char_register_types(void);
void vmx_port_register_types(void);


type_init(vmx_port_register_types)
type_init(char_register_types)
type_init(vmsvga_register_types)
type_init(vmport_register_types)
type_init(vmmouse_register_types)
type_init(vga_register_types)
type_init(sysbus_register_types)
type_init(ich9_smb_register);
type_init(smbus_eeprom_register_types)
type_init(smbus_device_register_types)
//type_init(serial_pci_register_types)
//type_init(serial_register_types)
//type_init(scsi_generic_register_types)
type_init(scsi_disk_register_types)
type_init(scsi_register_types)
type_init(rtl8139_register_types)
type_init(qdev_register_types)
type_init(ide_register_types)
type_init(platform_bus_register_types)
type_init(piix4_pm_register_types)
type_init(piix_ide_register_types)
type_init(i440fx_register_types)
type_init(pcspk_register)
type_init(i8042_register_types)
type_init(veertu_pci_register_types);
//type_init(pci_bridge_dev_register);
type_init(pci_bridge_register_types)
type_init(pci_register_types)
type_init(pci_ide_register_types)
type_init(pc_machine_register_types)
type_init(port92_register_types)
type_init(megasas_register_types)
type_init(mc146818rtc_register_types)
type_init(veertu_machine_register)
type_init(lsi53c895a_register_types)
type_init(ich9_lpc_register);
type_init(isa_ide_register_types)
type_init(isabus_register_types)
type_init(irq_register_types)
type_init(ioapic_common_register_types)
type_init(ioapic_register_types)
type_init(ich_ahci_register_types)
type_init(d2pbr_register);
type_init(pic_common_register_types)
type_init(pic_register_types)
type_init(register_devices);
type_init(pit_register_types)
type_init(usb_host_register_types)
type_init(usb_host_register_legacy_types)
type_init(xhci_register_types)
type_init(uhci_register_types)
type_init(ohci_register_types)
type_init(ehci_sysbus_register_types)
type_init(ehci_pci_register_types)
type_init(fw_cfg_register_types)
//type_init(fw_path_provider_register_types)
type_init(e1000_register_types)
type_init(usb_msd_register_types)
type_init(usb_hub_register_types)
type_init(usb_audio_register_types)
type_init(x86_cpu_register_types);
type_init(cpu_register_types)
type_init(i2c_slave_register_types)
type_init(register_types);
type_init(usb_register_types)
type_init(apic_common_register_types)
type_init(apic_register_types);
type_init(sysbus_ahci_register_types);

type_init(icc_bus_register_types);
type_init(register_accel_types);


void acpi_register_config(void);
void pc_machine_init(void);

machine_init(pc_machine_init);
machine_init(acpi_register_config);

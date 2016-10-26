/*
 * ACPI implementation
 *
 * Copyright (c) 2006 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>
 */
#include "hw.h"
#include "ipc.h"
#include "apm.h"
#include "pm_smbus.h"
#include "pci.h"
#include "nacpi.h"
#include "sysemu.h"
#include "qemu/range.h"
#include "ioport.h"
#include "fw_cfg.h"
#include "address-spaces.h"
#include "acpipiix4.h"
#include "acpipcihp.h"
#include "acpi_dev_interface.h"

//#define DEBUG

#ifdef PIIX4_DEBUG
# define PIIX4_DPRINTF(format, ...)     printf(format, ## __VA_ARGS__)
#else
# define PIIX4_DPRINTF(format, ...)     do { } while (0)
#endif

#define GPE_BASE 0xafe0
#define GPE_LEN 4

struct pci_status {
    uint32_t up; /* deprecated, maintained for migration compatibility */
    uint32_t down;
};

typedef struct PIIX4PMState {
    /*< private >*/
    PCIDevice parent;
    /*< public >*/

    VeertuMemArea io;
    uint32_t io_base;

    VeertuMemArea io_gpe;
    ACPIREGS ar;

    APMState apm;

    PMSMBus smb;
    uint32_t smb_io_base;

    vmx_irq irq;
    vmx_irq smi_irq;
    int vmx_enabled;
    Notifier machine_ready;
    Notifier powerdown_notifier;

    AcpiPciHpState acpi_pci_hotplug;
    bool use_acpi_pci_hotplug;

    uint8_t disable_s3;
    uint8_t disable_s4;
    uint8_t s4_val;

} PIIX4PMState;

#define TYPE_PIIX4_PM "PIIX4_PM"

#define PIIX4_PM(obj) obj
static void piix4_acpi_system_hot_add_init(VeertuMemArea *parent,
                                           PCIBus *bus, PIIX4PMState *s);

#define ACPI_ENABLE 0xf1
#define ACPI_DISABLE 0xf0

static void pm_tmr_timer(ACPIREGS *ar)
{
    PIIX4PMState *s = container_of(ar, PIIX4PMState, ar);
    acpi_update_sci(&s->ar, s->irq);
}

static void apm_ctrl_changed(uint32_t val, void *arg)
{
    PIIX4PMState *s = arg;
    PCIDevice *d = PCI_DEVICE(s);

    /* ACPI specs 3.0, 4.7.2.5 */
    acpi_pm1_cnt_update(&s->ar, val == ACPI_ENABLE, val == ACPI_DISABLE);

    if (d->config[0x5b] & (1 << 1)) {
        if (s->smi_irq) {
            vmx_irq_raise(s->smi_irq);
        }
    }
}

static void pm_io_space_update(PIIX4PMState *s)
{
    PCIDevice *d = PCI_DEVICE(s);

    s->io_base = le32_to_cpu(*(uint32_t *)(d->config + 0x40));
    s->io_base &= 0xffc0;

    mem_area_set_enable(&s->io, d->config[0x80] & 1, true);
    mem_area_set_addr(&s->io, s->io_base);
    veertu_mem_referesh();
}

static void smbus_io_space_update(PIIX4PMState *s)
{
    PCIDevice *d = PCI_DEVICE(s);

    s->smb_io_base = le32_to_cpu(*(uint32_t *)(d->config + 0x90));
    s->smb_io_base &= 0xffc0;

    mem_area_set_enable(&s->smb.io, d->config[0xd2] & 1, true);
    mem_area_set_addr(&s->smb.io, s->smb_io_base);
    veertu_mem_referesh();
}

static void pm_write_config(PCIDevice *d,
                            uint32_t address, uint32_t val, int len)
{
    pci_default_write_config(d, address, val, len);
    if (range_covers_byte(address, len, 0x80) ||
        ranges_overlap(address, len, 0x40, 4)) {
        pm_io_space_update((PIIX4PMState *)d);
    }
    if (range_covers_byte(address, len, 0xd2) ||
        ranges_overlap(address, len, 0x90, 4)) {
        smbus_io_space_update((PIIX4PMState *)d);
    }
}

static int vmstate_acpi_post_load(void *opaque, int version_id)
{
    PIIX4PMState *s = opaque;

    pm_io_space_update(s);
    return 0;
}

#define VMSTATE_GPE_ARRAY(_field, _state)                            \
 {                                                                   \
     .name       = (stringify(_field)),                              \
     .version_id = 0,                                                \
     .info       = &vmstate_info_uint16,                             \
     .size       = sizeof(uint16_t),                                 \
     .flags      = VMS_SINGLE | VMS_POINTER,                         \
     .offset     = vmstate_offset_pointer(_state, _field, uint8_t),  \
 }

static const VMStateDescription vmstate_gpe = {
    .name = "gpe",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_GPE_ARRAY(sts, ACPIGPE),
        VMSTATE_GPE_ARRAY(en, ACPIGPE),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_pci_status = {
    .name = "pci_status",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(up, struct AcpiPciHpPciStatus),
        VMSTATE_UINT32(down, struct AcpiPciHpPciStatus),
        VMSTATE_END_OF_LIST()
    }
};

static int acpi_load_old(QEMUFile *f, void *opaque, int version_id)
{
    PIIX4PMState *s = opaque;
    int ret, i;
    uint16_t temp;

    ret = pci_device_load(PCI_DEVICE(s), f);
    if (ret < 0) {
        return ret;
    }
    vmx_get_be16s(f, &s->ar.pm1.evt.sts);
    vmx_get_be16s(f, &s->ar.pm1.evt.en);
    vmx_get_be16s(f, &s->ar.pm1.cnt.cnt);

    ret = vmstate_load_state(f, &vmstate_apm, &s->apm, 1);
    if (ret) {
        return ret;
    }

    timer_get(f, s->ar.tmr.timer);
    vmx_get_sbe64s(f, &s->ar.tmr.overflow_time);

    vmx_get_be16s(f, (uint16_t *)s->ar.gpe.sts);
    for (i = 0; i < 3; i++) {
        vmx_get_be16s(f, &temp);
    }

    vmx_get_be16s(f, (uint16_t *)s->ar.gpe.en);
    for (i = 0; i < 3; i++) {
        vmx_get_be16s(f, &temp);
    }

    ret = vmstate_load_state(f, &vmstate_pci_status,
        &s->acpi_pci_hotplug.acpi_pcihp_pci_status[ACPI_PCIHP_BSEL_DEFAULT], 1);
    return ret;
}

static bool vmstate_test_use_acpi_pci_hotplug(void *opaque, int version_id)
{
    PIIX4PMState *s = opaque;
    return s->use_acpi_pci_hotplug;
}

static bool vmstate_test_no_use_acpi_pci_hotplug(void *opaque, int version_id)
{
    PIIX4PMState *s = opaque;
    return !s->use_acpi_pci_hotplug;
}

static bool vmstate_test_use_memhp(void *opaque)
{
    PIIX4PMState *s = opaque;
    return false;
}

/* qemu-kvm 1.2 uses version 3 but advertised as 2
 * To support incoming qemu-kvm 1.2 migration, change version_id
 * and minimum_version_id to 2 below (which breaks migration from
 * qemu 1.2).
 *
 */
static const VMStateDescription vmstate_acpi = {
    .name = "piix4_pm",
    .version_id = 3,
    .minimum_version_id = 3,
    .minimum_version_id_old = 1,
    .load_state_old = acpi_load_old,
    .post_load = vmstate_acpi_post_load,
    .fields = (VMStateField[]) {
        VMSTATE_PCI_DEVICE(parent, PIIX4PMState),
        VMSTATE_UINT16(ar.pm1.evt.sts, PIIX4PMState),
        VMSTATE_UINT16(ar.pm1.evt.en, PIIX4PMState),
        VMSTATE_UINT16(ar.pm1.cnt.cnt, PIIX4PMState),
        VMSTATE_STRUCT(apm, PIIX4PMState, 0, vmstate_apm, APMState),
        VMSTATE_TIMER(ar.tmr.timer, PIIX4PMState),
        VMSTATE_INT64(ar.tmr.overflow_time, PIIX4PMState),
        VMSTATE_STRUCT(ar.gpe, PIIX4PMState, 2, vmstate_gpe, ACPIGPE),
        VMSTATE_STRUCT_TEST(
            acpi_pci_hotplug.acpi_pcihp_pci_status[ACPI_PCIHP_BSEL_DEFAULT],
            PIIX4PMState,
            vmstate_test_no_use_acpi_pci_hotplug,
            2, vmstate_pci_status,
            struct AcpiPciHpPciStatus),
        VMSTATE_PCI_HOTPLUG(acpi_pci_hotplug, PIIX4PMState,
                            vmstate_test_use_acpi_pci_hotplug),
        VMSTATE_END_OF_LIST()
    },
    .subsections = (VMStateSubsection[]) {
        {
            .needed = vmstate_test_use_memhp,
        },
        VMSTATE_END_OF_LIST()
    }
};

static void piix4_reset(void *opaque)
{
    PIIX4PMState *s = opaque;
    PCIDevice *d = PCI_DEVICE(s);
    uint8_t *pci_conf = d->config;

    pci_conf[0x58] = 0;
    pci_conf[0x59] = 0;
    pci_conf[0x5a] = 0;
    pci_conf[0x5b] = 0;

    pci_conf[0x40] = 0x01; /* PM io base read only bit */
    pci_conf[0x80] = 0;

    if (s->vmx_enabled) {
        /* Mark SMM as already inited (until KVM supports SMM). */
        pci_conf[0x5B] = 0x02;
    }
    pm_io_space_update(s);
    acpi_pcihp_reset(&s->acpi_pci_hotplug);
}

static void piix4_pm_powerdown_req(Notifier *n, void *opaque)
{
    PIIX4PMState *s = container_of(n, PIIX4PMState, powerdown_notifier);

    assert(s != NULL);
    acpi_pm1_evt_power_down(&s->ar);
}

static void piix4_pm_machine_ready(Notifier *n, void *opaque)
{
    PIIX4PMState *s = container_of(n, PIIX4PMState, machine_ready);
    PCIDevice *d = PCI_DEVICE(s);
    VeertuMemArea *io_as = pci_address_space_io(d);
    uint8_t *pci_conf;

    pci_conf = d->config;
    pci_conf[0x5f] = 0x10 |
        (is_addr_in_mem_area(io_as, 0x378) ? 0x80 : 0);
    pci_conf[0x63] = 0x60;
    pci_conf[0x67] = (is_addr_in_mem_area(io_as, 0x3f8) ? 0x08 : 0) |
        (is_addr_in_mem_area(io_as, 0x2f8) ? 0x90 : 0);
}

void pro_get_uint16t_ptr(VeertuType *obj, Visitor *visitor, void *opaque, char *name, Error **junk)
{
    uint16_t val;
    
    val = *(uint16_t *)opaque;
    visit_type_uint16(visitor, &val, name, junk);
}

static void piix4_pm_add_propeties(PIIX4PMState *s)
{
    static const uint8_t acpi_enable_cmd = ACPI_ENABLE;
    static const uint8_t acpi_disable_cmd = ACPI_DISABLE;
    static const uint32_t gpe0_blk = GPE_BASE;
    static const uint32_t gpe0_blk_len = GPE_LEN;
    static const uint16_t sci_int = 9;
}

static int piix4_pm_initfn(PCIDevice *dev)
{
    PIIX4PMState *s = PIIX4_PM(dev);
    uint8_t *pci_conf;

    pci_conf = dev->config;
    pci_conf[0x06] = 0x80;
    pci_conf[0x07] = 0x02;
    pci_conf[0x09] = 0x00;
    pci_conf[0x3d] = 0x01; // interrupt pin 1

    /* APM */
    apm_init(dev, &s->apm, apm_ctrl_changed, s);

    if (s->vmx_enabled) {
        /* Mark SMM as already inited to prevent SMM from running.  KVM does not
         * support SMM mode. */
        pci_conf[0x5B] = 0x02;
    }

    /* XXX: which specification is used ? The i82731AB has different
       mappings */
    pci_conf[0x90] = s->smb_io_base | 1;
    pci_conf[0x91] = s->smb_io_base >> 8;
    pci_conf[0xd2] = 0x09;
    pm_smbus_init(DEVICE(dev), &s->smb);
    mem_area_set_enable(&s->smb.io, pci_conf[0xd2] & 1, true);
    mem_area_add_child(pci_address_space_io(dev),
                                s->smb_io_base, &s->smb.io);

    memory_area_init(&s->io,  "piix4-pm", 64);
    mem_area_set_enable(&s->io, false, true);
    mem_area_add_child(pci_address_space_io(dev),
                                0, &s->io);

    acpi_pm_tmr_init(&s->ar, pm_tmr_timer, &s->io);
    acpi_pm1_evt_init(&s->ar, pm_tmr_timer, &s->io);
    acpi_pm1_cnt_init(&s->ar, &s->io, s->s4_val);
    acpi_gpe_init(&s->ar, GPE_LEN);

    s->powerdown_notifier.notify = piix4_pm_powerdown_req;
    vmx_register_powerdown_notifier(&s->powerdown_notifier);

    s->machine_ready.notify = piix4_pm_machine_ready;
    vmx_add_machine_init_done_notifier(&s->machine_ready);
    vmx_register_reset(piix4_reset, s);

    piix4_acpi_system_hot_add_init(pci_address_space_io(dev), dev->bus, s);

    piix4_pm_add_propeties(s);
    return 0;
}

VeertuType *piix4_pm_find(void)
{
    return NULL;
}

I2CBus *piix4_pm_init(PCIBus *bus, int devfn, uint32_t smb_io_base,
                      vmx_irq sci_irq, vmx_irq smi_irq,
                      int vmx_enabled, FWCfgState *fw_cfg,
                      DeviceState **piix4_pm)
{
    DeviceState *dev;
    PIIX4PMState *s;

    dev = DEVICE(pci_create(bus, devfn, TYPE_PIIX4_PM));
    if (piix4_pm) {
        *piix4_pm = dev;
    }

    s = PIIX4_PM(dev);
    
    s->smb_io_base = smb_io_base;
    s->disable_s3 = 1;
    s->disable_s4 = 1;
    s->s4_val = 2;
    s->use_acpi_pci_hotplug = 1;
    
    s->irq = sci_irq;
    s->smi_irq = smi_irq;
    s->vmx_enabled = vmx_enabled;

    qdev_init_nofail(dev);

    if (fw_cfg) {
        uint8_t suspend[6] = {128, 0, 0, 129, 128, 128};
        suspend[3] = 1 | ((!s->disable_s3) << 7);
        suspend[4] = s->s4_val | ((!s->disable_s4) << 7);

        fw_cfg_add_file(fw_cfg, "etc/system-states", g_memdup(suspend, 6), 6);
    }

    return s->smb.smbus;
}

static uint64_t gpe_readb(void *opaque, hwaddr addr, unsigned width)
{
    PIIX4PMState *s = opaque;
    uint32_t val = acpi_gpe_ioport_readb(&s->ar, addr);

    PIIX4_DPRINTF("gpe read %" HWADDR_PRIx " == %" PRIu32 "\n", addr, val);
    return val;
}

static void gpe_writeb(void *opaque, hwaddr addr, uint64_t val,
                       unsigned width)
{
    PIIX4PMState *s = opaque;

    acpi_gpe_ioport_writeb(&s->ar, addr, val);
    acpi_update_sci(&s->ar, s->irq);

    PIIX4_DPRINTF("gpe write %" HWADDR_PRIx " <== %" PRIu64 "\n", addr, val);
}

static const MemAreaOps piix4_gpe_ops = {
    .read = gpe_readb,
    .write = gpe_writeb,
    .valid.min_access_size = 1,
    .valid.max_access_size = 4,
    .impl.min_access_size = 1,
    .impl.max_access_size = 1,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void piix4_acpi_system_hot_add_init(VeertuMemArea *parent,
                                           PCIBus *bus, PIIX4PMState *s)
{
    memory_area_init_io(&s->io_gpe, VeertuTypeHold(s), &piix4_gpe_ops, s,
                          "acpi-gpe0", GPE_LEN);
    mem_area_add_child(parent, GPE_BASE, &s->io_gpe);

    acpi_pcihp_init(&s->acpi_pci_hotplug, bus, parent,
                    s->use_acpi_pci_hotplug);
}

static void piix4_ospm_status(AcpiDeviceIf *adev, ACPIOSTInfoList ***list)
{
    PIIX4PMState *s = PIIX4_PM(adev);
}

static void piix4_pm_class_init(VeertuTypeClassHold *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);
    AcpiDeviceIfClass *adevc = ACPI_DEVICE_IF_CLASS(klass);

    k->init = piix4_pm_initfn;
    k->config_write = pm_write_config;
    k->vendor_id = PCI_VENDOR_ID_INTEL;
    k->device_id = PCI_DEVICE_ID_INTEL_82371AB_3;
    k->revision = 0x03;
    k->class_id = PCI_CLASS_BRIDGE_OTHER;
    dc->desc = "PM";
    dc->vmsd = &vmstate_acpi;
    /*
     * Reason: part of PIIX4 southbridge, needs to be wired up,
     * e.g. by mips_malta_init()
     */
    dc->cannot_instantiate_with_device_add_yet = true;
    dc->hotpluggable = false;
    adevc->ospm_status = piix4_ospm_status;
}

static const VeertuTypeInfo piix4_pm_info = {
    .name          = TYPE_PIIX4_PM,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(PIIX4PMState),
    .class_init    = piix4_pm_class_init,
};

void piix4_pm_register_types(void)
{
    register_type_internal(&piix4_pm_info);
}

/*
 *  APIC support - common bits of emulated and KVM kernel model
 *
 *  Copyright (c) 2004-2005 Fabrice Bellard
 *  Copyright (c) 2011      Jan Kiszka, Siemens AG
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
#include "iapic.h"
#include "iapic_internal.h"
#include "veertuemu.h"
#include "nqdev.h"
#include "qsysbus.h"

static int apic_irq_delivered;
bool apic_report_tpr_access;


uint8_t* cpu_get_apic_vmx_page(DeviceState *dev)
{
    if (dev) {
        APICCommonState *s = APIC_COMMON(dev);
        return s->vmx_apic_page;
    } else {
        return NULL;
    }
}

void cpu_set_apic_base(DeviceState *dev, uint64_t val)
{
    if (dev) {
        APICCommonState *s = APIC_COMMON(dev);
        APICCommonClass *info = APIC_COMMON_GET_CLASS(s);
        info->set_base(s, val);
    }
}

uint64_t cpu_get_apic_base(DeviceState *dev)
{
    if (dev) {
        APICCommonState *s = APIC_COMMON(dev);
        return s->apicbase;
    } else {
        return MSR_IA32_APICBASE_BSP;
    }
}

int apic_get_highest_priority_irr(DeviceState *dev)
{
    APICCommonState *s;
    APICCommonClass *info;

    if (!dev) {
        return -1;
    }
    s = APIC_COMMON(dev);
    info = APIC_COMMON_GET_CLASS(s);
    return info->get_highest_priority_irr(s);
}

void cpu_set_apic_tpr(DeviceState *dev, uint8_t val)
{
    APICCommonState *s;
    APICCommonClass *info;

    if (!dev) {
        return;
    }

    s = APIC_COMMON(dev);
    info = APIC_COMMON_GET_CLASS(s);

    info->set_tpr(s, val);
}

uint8_t cpu_get_apic_tpr(DeviceState *dev)
{
    APICCommonState *s;
    APICCommonClass *info;

    if (!dev) {
        return 0;
    }

    s = APIC_COMMON(dev);
    info = APIC_COMMON_GET_CLASS(s);

    return info->get_tpr(s);
}

void apic_enable_tpr_access_reporting(DeviceState *dev, bool enable)
{
    APICCommonState *s = APIC_COMMON(dev);
    APICCommonClass *info = APIC_COMMON_GET_CLASS(s);

    apic_report_tpr_access = enable;
    if (info->enable_tpr_reporting) {
        info->enable_tpr_reporting(s, enable);
    }
}

void apic_enable_vapic(DeviceState *dev, hwaddr paddr)
{
    APICCommonState *s = APIC_COMMON(dev);
    APICCommonClass *info = APIC_COMMON_GET_CLASS(s);

    s->vapic_paddr = paddr;
    info->vapic_base_update(s);
}

void apic_handle_tpr_access_report(DeviceState *dev, target_ulong ip,
                                   TPRAccess access)
{
    APICCommonState *s = APIC_COMMON(dev);

    //vapic_report_tpr_access(s->vapic, GETCPU(s->cpu), ip, access);
}

void apic_report_irq_delivered(int delivered)
{
    apic_irq_delivered += delivered;
}

void apic_reset_irq_delivered(void)
{
    /* Copy this into a local variable to encourage gcc to emit a plain
     * register for a sys/sdt.h marker.  For details on this workaround, see:
     * https://sourceware.org/bugzilla/show_bug.cgi?id=13296
     */
    volatile int a_i_d = apic_irq_delivered;

    apic_irq_delivered = 0;
}

int apic_get_irq_delivered(void)
{
    return apic_irq_delivered;
}

void apic_deliver_nmi(DeviceState *dev)
{
    APICCommonState *s = APIC_COMMON(dev);
    APICCommonClass *info = APIC_COMMON_GET_CLASS(s);

    info->external_nmi(s);
}

bool apic_next_timer(APICCommonState *s, int64_t current_time)
{
    int64_t d;

    /* We need to store the timer state separately to support APIC
     * implementations that maintain a non-QEMU timer, e.g. inside the
     * host kernel. This open-coded state allows us to migrate between
     * both models. */
    s->timer_expiry = -1;

    if (s->lvt[APIC_LVT_TIMER] & APIC_LVT_MASKED) {
        return false;
    }

    d = (current_time - s->initial_count_load_time) >> s->count_shift;

    if (s->lvt[APIC_LVT_TIMER] & APIC_LVT_TIMER_PERIODIC) {
        if (!s->initial_count) {
            return false;
        }
        d = ((d / ((uint64_t)s->initial_count + 1)) + 1) *
            ((uint64_t)s->initial_count + 1);
    } else {
        if (d >= s->initial_count) {
            return false;
        }
        d = (uint64_t)s->initial_count + 1;
    }
    s->next_time = s->initial_count_load_time + (d << s->count_shift);
    s->timer_expiry = s->next_time;
    return true;
}

void apic_init_reset(DeviceState *dev)
{
    APICCommonState *s = APIC_COMMON(dev);
    APICCommonClass *info = APIC_COMMON_GET_CLASS(s);
    int i;

    if (!s) {
        return;
    }
    s->tpr = 0;
    s->spurious_vec = 0xff;
    s->log_dest = 0;
    s->dest_mode = 0xf;
    memset(s->isr, 0, sizeof(s->isr));
    memset(s->tmr, 0, sizeof(s->tmr));
    memset(s->irr, 0, sizeof(s->irr));
    for (i = 0; i < APIC_LVT_NB; i++) {
        s->lvt[i] = APIC_LVT_MASKED;
    }
    s->esr = 0;
    memset(s->icr, 0, sizeof(s->icr));
    s->divide_conf = 0;
    s->count_shift = 0;
    s->initial_count = 0;
    s->initial_count_load_time = 0;
    s->next_time = 0;
    s->wait_for_sipi = !cpu_is_bsp(s->cpu);

    if (s->timer) {
        timer_del(s->timer);
    }
    s->timer_expiry = -1;

    if (info->reset) {
        info->reset(s);
    }
}

void apic_designate_bsp(DeviceState *dev)
{
    if (dev == NULL) {
        return;
    }

    APICCommonState *s = APIC_COMMON(dev);
    s->apicbase |= MSR_IA32_APICBASE_BSP;
}

static void apic_reset_common(DeviceState *dev)
{
    APICCommonState *s = APIC_COMMON(dev);
    APICCommonClass *info = APIC_COMMON_GET_CLASS(s);
    bool bsp;

    bsp = cpu_is_bsp(s->cpu);
    s->apicbase = APIC_DEFAULT_ADDRESS |
        (bsp ? MSR_IA32_APICBASE_BSP : 0) | MSR_IA32_APICBASE_ENABLE;

    s->vapic_paddr = 0;
    info->vapic_base_update(s);

    apic_init_reset(dev);

    if (bsp) {
        /*
         * LINT0 delivery mode on GETCPU #0 is set to ExtInt at initialization
         * time typically by BIOS, so PIC interrupt can be delivered to the
         * processor when local APIC is enabled.
         */
        s->lvt[APIC_LVT_LINT0] = 0x700;
    }
}

/* This function is only used for old state version 1 and 2 */
static int apic_load_old(QEMUFile *f, void *opaque, int version_id)
{
    APICCommonState *s = opaque;
    APICCommonClass *info = APIC_COMMON_GET_CLASS(s);
    int i;

    if (version_id > 2) {
        return -EINVAL;
    }

    /* XXX: what if the base changes? (registered memory regions) */
    vmx_get_be32s(f, &s->apicbase);
    vmx_get_8s(f, &s->id);
    vmx_get_8s(f, &s->arb_id);
    vmx_get_8s(f, &s->tpr);
    vmx_get_be32s(f, &s->spurious_vec);
    vmx_get_8s(f, &s->log_dest);
    vmx_get_8s(f, &s->dest_mode);
    for (i = 0; i < 8; i++) {
        vmx_get_be32s(f, &s->isr[i]);
        vmx_get_be32s(f, &s->tmr[i]);
        vmx_get_be32s(f, &s->irr[i]);
    }
    for (i = 0; i < APIC_LVT_NB; i++) {
        vmx_get_be32s(f, &s->lvt[i]);
    }
    vmx_get_be32s(f, &s->esr);
    vmx_get_be32s(f, &s->icr[0]);
    vmx_get_be32s(f, &s->icr[1]);
    vmx_get_be32s(f, &s->divide_conf);
    s->count_shift = vmx_get_be32(f);
    vmx_get_be32s(f, &s->initial_count);
    s->initial_count_load_time = vmx_get_be64(f);
    s->next_time = vmx_get_be64(f);

    if (version_id >= 2) {
        s->timer_expiry = vmx_get_be64(f);
    }

    if (info->post_load) {
        info->post_load(s);
    }
    return 0;
}

static void apic_common_realize(DeviceState *dev, Error **errp)
{
    APICCommonState *s = APIC_COMMON(dev);
    APICCommonClass *info;
    static DeviceState *vapic;
    static int apic_no;
    static bool mmio_registered;

    if (apic_no >= MAX_APICS) {
        error_setg(errp, "%s initialization failed.",
                   get_typename(VeertuTypeHold(dev)));
        return;
    }
    s->idx = apic_no++;
    //s->id = -1;
    s->version = 0x14;
    s->vapic = 1;
    
    
    info = APIC_COMMON_GET_CLASS(s);
    info->realize(dev, errp);
    if (!mmio_registered) {
        ICCBus *b = ICC_BUS(qdev_get_parent_bus(dev));
        mem_area_add_child(b->apic_address_space, 0, &s->io_memory);
        mmio_registered = true;
    }

    s->vapic = NULL;
    if (apic_report_tpr_access && info->enable_tpr_reporting) {
        info->enable_tpr_reporting(s, true);
    }

}

static int apic_pre_load(void *opaque)
{
    APICCommonState *s = APIC_COMMON(opaque);

    /* The default is !cpu_is_bsp(s->cpu), but the common value is 0
     * so that's what apic_common_sipi_needed checks for.  Reset to
     * the value that is assumed when the apic_sipi subsection is
     * absent.
     */
    s->wait_for_sipi = 0;
    return 0;
}

static void apic_dispatch_pre_save(void *opaque)
{
    APICCommonState *s = APIC_COMMON(opaque);
    APICCommonClass *info = APIC_COMMON_GET_CLASS(s);

    if (info->pre_save) {
        info->pre_save(s);
    }
}

static int apic_dispatch_post_load(void *opaque, int version_id)
{
    APICCommonState *s = APIC_COMMON(opaque);
    APICCommonClass *info = APIC_COMMON_GET_CLASS(s);

    if (info->post_load) {
        info->post_load(s);
    }
    return 0;
}

static bool apic_common_sipi_needed(void *opaque)
{
    APICCommonState *s = APIC_COMMON(opaque);
    return s->wait_for_sipi != 0;
}

static const VMStateDescription vmstate_apic_common_sipi = {
    .name = "apic_sipi",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_INT32(sipi_vector, APICCommonState),
        VMSTATE_INT32(wait_for_sipi, APICCommonState),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_apic_common = {
    .name = "apic",
    .version_id = 3,
    .minimum_version_id = 3,
    .minimum_version_id_old = 1,
    .load_state_old = apic_load_old,
    .pre_load = apic_pre_load,
    .pre_save = apic_dispatch_pre_save,
    .post_load = apic_dispatch_post_load,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(apicbase, APICCommonState),
        VMSTATE_UINT8(id, APICCommonState),
        VMSTATE_UINT8(arb_id, APICCommonState),
        VMSTATE_UINT8(tpr, APICCommonState),
        VMSTATE_UINT32(spurious_vec, APICCommonState),
        VMSTATE_UINT8(log_dest, APICCommonState),
        VMSTATE_UINT8(dest_mode, APICCommonState),
        VMSTATE_UINT32_ARRAY(isr, APICCommonState, 8),
        VMSTATE_UINT32_ARRAY(tmr, APICCommonState, 8),
        VMSTATE_UINT32_ARRAY(irr, APICCommonState, 8),
        VMSTATE_UINT32_ARRAY(lvt, APICCommonState, APIC_LVT_NB),
        VMSTATE_UINT32(esr, APICCommonState),
        VMSTATE_UINT32_ARRAY(icr, APICCommonState, 2),
        VMSTATE_UINT32(divide_conf, APICCommonState),
        VMSTATE_INT32(count_shift, APICCommonState),
        VMSTATE_UINT32(initial_count, APICCommonState),
        VMSTATE_INT64(initial_count_load_time, APICCommonState),
        VMSTATE_INT64(next_time, APICCommonState),
        VMSTATE_INT64(timer_expiry,
                      APICCommonState), /* open-coded timer state */
        VMSTATE_BUFFER(vmx_apic_page, APICCommonState),
        VMSTATE_END_OF_LIST()
    },
    .subsections = (VMStateSubsection[]) {
        {
            .vmsd = &vmstate_apic_common_sipi,
            .needed = apic_common_sipi_needed,
        },
        VMSTATE_END_OF_LIST()
    }
};

static void apic_common_class_init(VeertuTypeClassHold *klass, void *data)
{
    ICCDeviceClass *idc = ICC_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->vmsd = &vmstate_apic_common;
    dc->reset = apic_reset_common;
    idc->realize = apic_common_realize;
    /*
     * Reason: APIC and GETCPU need to be wired up by
     * x86_cpu_apic_create()
     */
    dc->cannot_instantiate_with_device_add_yet = true;
}

static const VeertuTypeInfo apic_common_type = {
    .name = TYPE_APIC_COMMON,
    .parent = TYPE_ICC_DEVICE,
    .instance_size = sizeof(APICCommonState),
    .class_size = sizeof(APICCommonClass),
    .class_init = apic_common_class_init,
};

void apic_common_register_types(void)
{
    register_type_internal(&apic_common_type);
}
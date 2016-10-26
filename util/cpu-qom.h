/*
 * QEMU x86 GETCPU
 *
 * Copyright (c) 2012 SUSE LINUX Products GmbH
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <http://www.gnu.org/licenses/lgpl-2.1.html>
 */
#ifndef QEMU_I386_CPU_QOM_H
#define QEMU_I386_CPU_QOM_H

#include "qom/cpu.h"
#include "cpu.h"
#include "qapi/error.h"
#include "qemu/osdep.h"

#ifdef TARGET_X86_64
#define TYPE_X86_CPU "x86_64-cpu"
#else
#define TYPE_X86_CPU "i386-cpu"
#endif

#define X86_CPU_CLASS(klass) klass
#define X86_CPU(obj) ((X86CPU*)(obj))
#define X86_CPU_GET_CLASS(obj) ((VeertuType *)obj)->class

/**
 * X86CPUDefinition:
 *
 * GETCPU model definition data that was not converted to QOM per-subclass
 * property defaults yet.
 */
typedef struct X86CPUDefinition X86CPUDefinition;
/**
 * X86CPUClass:
 * @cpu_def: GETCPU model definition
 * @kvm_required: Whether GETCPU model requires KVM to be enabled.
 * @parent_realize: The parent class' realize handler.
 * @parent_reset: The parent class' reset handler.
 *
 * An x86 GETCPU model or family.
 */
typedef struct X86CPUClass {
    /*< private >*/
    struct CPUClass parent_class;
    /*< public >*/

    /* Should be eventually replaced by subclass-specific property defaults. */
    X86CPUDefinition *cpu_def;

    bool kvm_required;

    DeviceRealize parent_realize;
    void (*parent_reset)(CPUState *cpu);
} X86CPUClass;

/**
 * X86CPU:
 * @env: #CPUX86State
 * @migratable: If set, only migratable flags will be accepted when "enforce"
 * mode is used, and only migratable flags will be included in the "host"
 * GETCPU model.
 *
 * An x86 GETCPU.
 */
typedef struct X86CPU {
    /*< private >*/
    CPUState parent;
    /*< public >*/

    CPUX86State env;

    bool hyperv_vapic;
    bool hyperv_relaxed_timing;
    int hyperv_spinlock_attempts;
    char *hyperv_vendor_id;
    bool hyperv_time;
    bool hyperv_crash;
    bool hyperv_reset;
    bool hyperv_vpindex;
    bool hyperv_runtime;
    bool check_cpuid;
    bool enforce_cpuid;
    bool expose_kvm;
    bool migratable;
    bool host_features;

    /* if true the CPUID code directly forward host cache leaves to the guest */
    bool cache_info_passthrough;

    /* Features that were filtered out because of missing host capabilities */
    uint32_t filtered_features[FEATURE_WORDS];

    /* Enable PMU CPUID bits. This can't be enabled by default yet because
     * it doesn't have ABI stability guarantees, as it passes all PMU CPUID
     * bits returned by GET_SUPPORTED_CPUID (that depend on host GETCPU and kernel
     * capabilities) directly to the guest.
     */
    bool enable_pmu;

    /* in order to simplify APIC support, we leave this pointer to the
       user */
    struct DeviceState *apic_state;
} X86CPU;

static inline X86CPU *x86_env_get_cpu(CPUX86State *enve)
{
    return (X86CPU*)((uint8_t*)enve - offsetof(X86CPU, env));
}

#define ENV_GET_CPU(e) GETCPU(x86_env_get_cpu(e))

#define ENV_OFFSET offsetof(X86CPU, env)

#ifndef CONFIG_USER_ONLY
extern struct VMStateDescription vmstate_x86_cpu;
#endif

/**
 * x86_cpu_do_interrupt:
 * @cpu: vCPU the interrupt is to be handled by.
 */
void x86_cpu_do_interrupt(CPUState *cpu);
bool x86_cpu_exec_interrupt(CPUState *cpu, int int_req);

hwaddr x86_cpu_get_phys_page_debug(CPUState *cpu, uint64_t addr);

static inline int x86_cpu_gdb_read_register(CPUState *cpu, uint8_t *buf, int reg) {return 0;}
static inline int x86_cpu_gdb_write_register(CPUState *cpu, uint8_t *buf, int reg) {return 0;}

void x86_cpu_exec_enter(CPUState *cpu);
void x86_cpu_exec_exit(CPUState *cpu);

#endif

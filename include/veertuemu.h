#ifndef __VEERTU_H__
#define __VEERTU_H__

#include <errno.h>
#include "config-host.h"
#include "qemu/queue.h"
#include "qom/cpu.h"
#

//#if defined CONFIG_VEERTU || !defined NEED_CPU_H
#define vmx_enabled()           (1)

//#else
//#define vmx_enabled()           (0)
//#endif

struct VeertuState;
typedef struct VeertuState VeertuState;
extern VeertuState *veertu_state;

//#ifdef NEED_CPU_H

void veertu_inject_interrupts(CPUState *cpu);

int veertu_process_events(CPUState *cpu);

int veertu_get_registers(CPUState *cpu);

/* state subset only touched by the VCPU itself during runtime */
#define VEERTU_PUT_RUNTIME_STATE   1
/* state subset modified during VCPU reset */
#define VEERTU_PUT_RESET_STATE     2
/* full state set, modified during initialization or on vmload */
#define VEERTU_PUT_FULL_STATE      3

/* Returns VCPU ID to be used on KVM_CREATE_VCPU ioctl() */
unsigned long veertu_vcpu_id(CPUState *cpu);

uint32_t veertu_get_supported_cpuid(VeertuState *env, uint32_t function,
                                      uint32_t index, int reg);

//#endif /* NEED_CPU_H */

void veertu_cpu_synchronize_state(CPUState *cpu);
void veertu_cpu_synchronize_post_reset(CPUState *cpu);
void veertu_cpu_synchronize_post_init(CPUState *cpu);
void veertu_cpu_clean_state(CPUState *cpu_state);

/* generic hooks - to be moved/refactored once there are more users */

static inline void cpu_synchronize_state(CPUState *cpu)
{
    if (vmx_enabled()) {
        veertu_cpu_synchronize_state(cpu);
    }
}

static inline void cpu_synchronize_post_reset(CPUState *cpu)
{
    if (vmx_enabled()) {
        veertu_cpu_synchronize_post_reset(cpu);
    }
}

static inline void cpu_synchronize_post_init(CPUState *cpu)
{
    if (vmx_enabled()) {
        veertu_cpu_synchronize_post_init(cpu);
    }
}

static inline void cpu_clean_state(CPUState *cpu)
{
    if (vmx_enabled()) {
        veertu_cpu_clean_state(cpu);
    }
}

int veertu_cpu_exec(CPUState *cpu);

#endif
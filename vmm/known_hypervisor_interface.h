
/* Microsoft Compatabile Hypervisor Interface support */

#ifndef __KNOWN_HYPERVISOR_INTERFACE__
#define __KNOWN_HYPERVISOR_INTERFACE__

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdarg.h>
#include "qemu-common.h"
//#include "x86_gen.h"
#include "vmm/x86_flags.h"

struct known_hypervisor_interface {

    void (*init_cpu_context)(struct CPUState* cpu);

    void (*cpuid_handler)(
            struct CPUState* cpu, 
            int func, int cnt, 
            uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx); 
    
    void (*wrmsr_handler)(
            struct CPUState* cpu,
            uint32_t msr, uint64_t value);

    uint64_t (*rdmsr_handler)(
            struct CPUState* cpu,
            uint32_t msr);

    void (*hypercall_handler)(struct CPUState* cpu);
};

extern struct known_hypervisor_interface* g_hypervisor_iface;

#endif /* __KNOWN_HYPERVISOR_INTERFACE__ */

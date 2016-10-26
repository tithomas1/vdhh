#ifndef CPU_H
#define CPU_H

#include "qdev-core.h"
#include "hwaddr.h"
#include "qemu/tls.h"
#include "qemu/thread.h"
#include "qemu/typedefs.h"
#include "../../vmm/x86.h"
#include <Hypervisor/hv_types.h>


#define GETCPU(object) ((CPUState *)(object))
#define CPU_GET_CLASS(object) ((VeertuType *)object)->class
#define CPU_CLASS(class) class

struct VeertuState;

struct CPUState {
    DeviceState parent;
    struct QemuThread *thread;
    int thread_id;
    struct QemuCond *halt_cond;
    int running;
    struct vmx_work_item *queued_work_first;
    struct vmx_work_item *queued_work_last;
    int num_cores;
    int num_threads;
    int thread_kicked;
    int created;
    int stop;
    int stopped;
    VeertuAddressSpace *as;
    void *env_ptr;
    sig_atomic_t exit_request;
    QTAILQ_ENTRY(CPUState) node;
    hv_vcpuid_t mac_vcpu_fd;
    int hlt;
    uint64_t init_tsc;
    
    uint64_t vmx_cap_pinbased;
    uint64_t vmx_cap_procbased;
    uint64_t vmx_cap_procbased2;
    uint64_t vmx_cap_entry;
    int interruptable;
    uint64_t exp_rip;
    uint64_t fetch_rip;
    uint64_t rip;
    struct x86_register regs[16];
    struct x86_reg_flags   rflags;
    struct lazy_flags   lflags;
    struct x86_efer efer;
    uint8_t mmio_buf[4096];
    uint8_t* apic_page;
    
    bool vmx_vcpu_dirty;
    struct VeertuState *veertu_state;
    void *opaque;
    int can_do_io;
    uint32_t halted;
    int cpu_index;
    uint32_t interrupt_request;
    
    /* Used for "Known Hypervisor Interface" per-cpu context */
    void* hypervisor_iface_context;
};
typedef struct CPUState CPUState;

typedef struct CPUClass {
    DeviceClass parent_class;
    
    struct VMStateDescription *vmsd;
    
    void (*do_interrupt)(CPUState *cpu_state);
    void (*reset)(CPUState *cpu_state);
    void (*set_pc)(CPUState *cpu_state, uint64_t data);
    bool (*has_work)(CPUState *cpu_state);
} CPUClass;

#endif

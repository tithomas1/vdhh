#ifndef VMX_H
#define VMX_H

#include <stdint.h>
#include <Hypervisor/hv.h>
#include <Hypervisor/hv_vmx.h>
#include "vmcs.h"
#include "qemu/tls.h"
#include "address-spaces.h"
#include "util/cpu.h"

void vmx_init_vcpu(CPUState *cpu);
void cpu_resume(CPUState *cpu);
static void inline cpu_exit(CPUState *cpu)
{
    cpu->exit_request = true;
}
static void inline cpu_reset_interrupt(CPUState *cpu, int mask)
{
    cpu->interrupt_request &= ~mask;
}
void vmx_cpu_kick(CPUState *cpu);
bool vmx_cpu_is_self(CPUState *cpu);



static bool inline cpu_has_work(CPUState *cpu_state)
{
    CPUClass *cpu_class = CPU_GET_CLASS(cpu_state);
    return cpu_class->has_work(cpu_state);
}


static hwaddr inline cpu_get_phys_page_debug(CPUState *cpu_state, uint64_t addr)
{
    abort();
}

static void inline cpu_reset(CPUState *cpu)
{
    CPUClass *class = CPU_GET_CLASS(cpu);
    if (class->reset)
        (*class->reset)(cpu);
}

DECLARE_TLS(CPUState *, current_cpu);
#define current_cpu tls_var(current_cpu)

#define vmstate_cpu_common vmstate_dummy



typedef void (*CPUInterruptHandler)(CPUState *, int);
extern CPUInterruptHandler cpu_interrupt_handler;
static void inline cpu_interrupt(CPUState *cpu_state, int mask)
{
    cpu_interrupt_handler(cpu_state, mask);
}
void async_run_on_cpu(CPUState *cpu, void (*func)(void *data), void *data);
CPUState *vmx_get_cpu(int index);
void run_on_cpu(CPUState *cpu, void (*func)(void *data), void *data);

extern struct CPUTailQ cpus;
QTAILQ_HEAD(CPUTailQ, CPUState);
#define first_cpu QTAILQ_FIRST(&cpus)
#define CPU_FOREACH(cpu_state) QTAILQ_FOREACH(cpu_state, &cpus, node)

static uint64_t inline rreg(hv_vcpuid_t vcpu, hv_x86_reg_t reg)
{
	uint64_t v;

	if (hv_vcpu_read_register(vcpu, reg, &v)) {
		abort();
	}

	return v;
}

/* write GPR */
static void inline wreg(hv_vcpuid_t vcpu, hv_x86_reg_t reg, uint64_t v)
{
	if (hv_vcpu_write_register(vcpu, reg, v)) {
		abort();
	}
}

/* read VMCS field */
static uint64_t inline rvmcs(hv_vcpuid_t vcpu, uint32_t field)
{
	uint64_t v;

	if (hv_vmx_vcpu_read_vmcs(vcpu, field, &v)) {
		abort();
	}

	return v;
}

/* write VMCS field */
static void inline wvmcs(hv_vcpuid_t vcpu, uint32_t field, uint64_t v)
{
	if (hv_vmx_vcpu_write_vmcs(vcpu, field, v)) {
		abort();
	}
}

/* desired control word constrained by hardware/hypervisor capabilities */
static uint64_t inline cap2ctrl(uint64_t cap, uint64_t ctrl)
{
	return (ctrl | (cap & 0xffffffff)) & (cap >> 32);
}

#define VM_ENTRY_GUEST_LMA (1LL << 9)

#define AR_TYPE_ACCESSES_MASK 1
#define AR_TYPE_READABLE_MASK (1 << 1)
#define AR_TYPE_WRITEABLE_MASK (1 << 2)
#define AR_TYPE_CODE_MASK (1 << 3)
#define AR_TYPE_MASK 0x0f
#define AR_TYPE_BUSY_64_TSS 11
#define AR_TYPE_BUSY_32_TSS 11
#define AR_TYPE_BUSY_16_TSS 3
#define AR_TYPE_LDT 2

static void enter_long_mode(hv_vcpuid_t vcpu, uint64_t cr0, uint64_t efer)
{
    uint64_t entry_ctls;

    efer |= EFER_LMA;
    wvmcs(vcpu, VMCS_GUEST_IA32_EFER, efer);
    entry_ctls = rvmcs(vcpu, VMCS_ENTRY_CTLS);
    wvmcs(vcpu, VMCS_ENTRY_CTLS, rvmcs(vcpu, VMCS_ENTRY_CTLS) | VM_ENTRY_GUEST_LMA);

    uint64_t guest_tr_ar = rvmcs(vcpu, VMCS_GUEST_TR_ACCESS_RIGHTS);
    if ((efer & EFER_LME) && (guest_tr_ar & AR_TYPE_MASK) != AR_TYPE_BUSY_64_TSS) {
        wvmcs(vcpu, VMCS_GUEST_TR_ACCESS_RIGHTS, (guest_tr_ar & ~AR_TYPE_MASK) | AR_TYPE_BUSY_64_TSS);
    }
}

static void exit_long_mode(hv_vcpuid_t vcpu, uint64_t cr0, uint64_t efer)
{
    uint64_t entry_ctls;

    entry_ctls = rvmcs(vcpu, VMCS_ENTRY_CTLS);
    wvmcs(vcpu, VMCS_ENTRY_CTLS, entry_ctls & ~VM_ENTRY_GUEST_LMA);
   
    efer &= ~EFER_LMA;
    wvmcs(vcpu, VMCS_GUEST_IA32_EFER, efer);
}
bool address_space_rw(VeertuAddressSpace *address_space, hwaddr addr, uint8_t *buf, int len, bool is_write);
static void inline macvm_set_cr0(hv_vcpuid_t vcpu, uint64_t cr0)
{
    int i;
    uint64_t pdpte[4] = {0, 0, 0, 0};
    uint64_t efer = rvmcs(vcpu, VMCS_GUEST_IA32_EFER);
    uint64_t old_cr0 = rvmcs(vcpu, VMCS_GUEST_CR0);

    if ((cr0 & CR0_PG) && (rvmcs(vcpu, VMCS_GUEST_CR4) & CR4_PAE) && !(efer & EFER_LME))
        address_space_rw(&address_space_memory, rvmcs(vcpu, VMCS_GUEST_CR3) & ~0x1f, (uint8_t *)pdpte, 32, 0);

    for (i = 0; i < 4; i++)
        wvmcs(vcpu, VMCS_GUEST_PDPTE0 + i * 2, pdpte[i]);

    wvmcs(vcpu, VMCS_CR0_MASK, CR0_CD | CR0_NE | CR0_PG);
    wvmcs(vcpu, VMCS_CR0_SHADOW, cr0);

    cr0 &= ~CR0_CD;
    wvmcs(vcpu, VMCS_GUEST_CR0, cr0 | CR0_NE| CR0_ET);

    if (efer & EFER_LME) {
        if (!(old_cr0 & CR0_PG) && (cr0 & CR0_PG))
             enter_long_mode(vcpu, cr0, efer);
        if (/*(old_cr0 & CR0_PG) &&*/ !(cr0 & CR0_PG))
            exit_long_mode(vcpu, cr0, efer);
    }

    hv_vcpu_invalidate_tlb(vcpu);
    hv_vcpu_flush(vcpu);
}

static void inline macvm_set_cr4(hv_vcpuid_t vcpu, uint64_t cr4)
{
    uint64_t guest_cr4 = cr4 | CR4_VMXE;

    wvmcs(vcpu, VMCS_GUEST_CR4, guest_cr4);
    wvmcs(vcpu, VMCS_CR4_SHADOW, cr4);

    hv_vcpu_invalidate_tlb(vcpu);
    hv_vcpu_flush(vcpu);
}

static void inline macvm_set_rip(CPUState *cpu, uint64_t rip)
{
    uint64_t val;

    /* BUG, should take considering overlap.. */
    wreg(cpu->mac_vcpu_fd, HV_X86_RIP, rip);

    /* after moving forward in rip, we need to clean INTERRUPTABILITY */
   val = rvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_INTERRUPTIBILITY);
   if (val & (VMCS_INTERRUPTIBILITY_STI_BLOCKING | VMCS_INTERRUPTIBILITY_MOVSS_BLOCKING))
        wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_INTERRUPTIBILITY,
              val & ~(VMCS_INTERRUPTIBILITY_STI_BLOCKING | VMCS_INTERRUPTIBILITY_MOVSS_BLOCKING));
}

static void inline vmx_clear_nmi_blocking(CPUState *cpu)
{
    uint32_t gi = (uint32_t) rvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_INTERRUPTIBILITY);
    gi &= ~VMCS_INTERRUPTIBILITY_NMI_BLOCKING;
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_INTERRUPTIBILITY, gi);
}

static void inline vmx_set_nmi_blocking(CPUState *cpu)
{
    uint32_t gi = (uint32_t)rvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_INTERRUPTIBILITY);
    gi |= VMCS_INTERRUPTIBILITY_NMI_BLOCKING;
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_INTERRUPTIBILITY, gi);
}

static void inline vmx_set_nmi_window_exiting(CPUState *cpu)
{
    uint64_t val;
    val = rvmcs(cpu->mac_vcpu_fd, VMCS_PRI_PROC_BASED_CTLS);
    wvmcs(cpu->mac_vcpu_fd, VMCS_PRI_PROC_BASED_CTLS, val | VMCS_PRI_PROC_BASED_CTLS_NMI_WINDOW_EXITING);

}

static void inline vmx_clear_nmi_window_exiting(CPUState *cpu)
{
    
    uint64_t val;
    val = rvmcs(cpu->mac_vcpu_fd, VMCS_PRI_PROC_BASED_CTLS);
    wvmcs(cpu->mac_vcpu_fd, VMCS_PRI_PROC_BASED_CTLS, val & ~VMCS_PRI_PROC_BASED_CTLS_NMI_WINDOW_EXITING);
}

#endif

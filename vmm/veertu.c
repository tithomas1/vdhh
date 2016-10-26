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

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <Hypervisor/hv.h>
#include <Hypervisor/hv_vmx.h>
#include "vmcs.h"
#include "vmx.h"
#include "x86_decode.h"
#include "x86_emu.h"
#include "x86_mmu.h"
#include "x86_cpuid.h"
#include "x86_descr.h"
#include "qemu-common.h"
#include "sysemu.h"
#include "veertuemu.h"
#include "qemu/option.h"
#include "emuaccel.h"
#include "hw.h"
#include "ui/console.h"
#include "boards.h"
#include "known_hypervisor_interface.h"

typedef struct VeertuSlot {
    uint64_t start;
    uint64_t size;
    uint8_t* mem;
    int slot_id;
} VeertuSlot;

struct VeertuState {
    AccelState parent;
    VeertuSlot slots[32];
    int num_slots;
};

pthread_rwlock_t mem_lock = PTHREAD_RWLOCK_INITIALIZER;
VeertuState *veertu_state;

VeertuSlot *veertu_find_overlap_slot(uint64_t start, uint64_t end)
{
    VeertuSlot *slot;
    int x;
    
    for (x = 0; x < veertu_state->num_slots; ++x) {
        slot = &veertu_state->slots[x];
        if (slot->size && start < (slot->start + slot->size) && end >= slot->start)
            return slot;
    }
    return NULL;
}

struct mac_slot {
    int present;
    uint64_t size;
    uint64_t gpa_start;
    uint64_t gva;
};

struct mac_slot slots[32];

static inline void mark_slot_page_dirty(VeertuSlot *slot, uint64_t addr)
{
}


#define ALIGN(x, y)  (((x)+(y)-1) & ~((y)-1))

int __veertu_set_memory(VeertuSlot *slot)
{
    struct mac_slot *macslot;
    hv_memory_flags_t flags;
    pthread_rwlock_wrlock(&mem_lock);

    macslot = &slots[slot->slot_id];

    if (macslot->present) {
        if (macslot->size != slot->size) {
            macslot->present = 0;
            if (hv_vm_unmap(macslot->gpa_start, macslot->size)) {
                printf("unmap failed\n");
                abort();
                return -1;
            }
        }
    }

    if (!slot->size) {
        pthread_rwlock_unlock(&mem_lock);
        return 0;
    }

    flags = HV_MEMORY_READ | HV_MEMORY_WRITE | HV_MEMORY_EXEC;
    
    macslot->present = 1;
    macslot->gpa_start = slot->start;
    macslot->size = slot->size;
    if (hv_vm_map((hv_uvaddr_t)slot->mem, slot->start, slot->size, flags)) {
        printf("register failed\n");
        abort();
        return -1;
    }
    pthread_rwlock_unlock(&mem_lock);
    return 0;
}

void vmx_reset_vcpu(CPUState *cpu)
{
    uint64_t msr = 0xfee00000 | MSR_IA32_APICBASE_ENABLE;
    if (cpu_is_bsp(X86_CPU(cpu))) {
        msr |= MSR_IA32_APICBASE_BSP;
    }

    wvmcs(cpu->mac_vcpu_fd, VMCS_ENTRY_CTLS, 0);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_IA32_EFER, 0);
    macvm_set_cr0(cpu->mac_vcpu_fd, 0x60000010);

    wvmcs(cpu->mac_vcpu_fd, VMCS_CR4_MASK, CR4_VMXE);
    wvmcs(cpu->mac_vcpu_fd, VMCS_CR4_SHADOW, 0x0);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_CR4, CR4_VMXE);

    // set VMCS guest state fields
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_CS_SELECTOR, 0xf000);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_CS_LIMIT, 0xffff);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_CS_ACCESS_RIGHTS, 0x9b);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_CS_BASE, 0xffff0000);

    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_DS_SELECTOR, 0);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_DS_LIMIT, 0xffff);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_DS_ACCESS_RIGHTS, 0x93);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_DS_BASE, 0);

    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_ES_SELECTOR, 0);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_ES_LIMIT, 0xffff);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_ES_ACCESS_RIGHTS, 0x93);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_ES_BASE, 0);

    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_FS_SELECTOR, 0);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_FS_LIMIT, 0xffff);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_FS_ACCESS_RIGHTS, 0x93);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_FS_BASE, 0);

    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_GS_SELECTOR, 0);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_GS_LIMIT, 0xffff);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_GS_ACCESS_RIGHTS, 0x93);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_GS_BASE, 0);

    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_SS_SELECTOR, 0);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_SS_LIMIT, 0xffff);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_SS_ACCESS_RIGHTS, 0x93);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_SS_BASE, 0);

    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_LDTR_SELECTOR, 0);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_LDTR_LIMIT, 0);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_LDTR_ACCESS_RIGHTS, 0x10000);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_LDTR_BASE, 0);

    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_TR_SELECTOR, 0);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_TR_LIMIT, 0);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_TR_ACCESS_RIGHTS, 0x83);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_TR_BASE, 0);
    
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_GDTR_LIMIT, 0);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_GDTR_BASE, 0);

    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_IDTR_LIMIT, 0);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_IDTR_BASE, 0);

    //wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_CR2, 0x0);
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_CR3, 0x0);

    wreg(cpu->mac_vcpu_fd, HV_X86_RIP, 0xfff0);
    wreg(cpu->mac_vcpu_fd, HV_X86_RDX, 0x623);
    wreg(cpu->mac_vcpu_fd, HV_X86_RFLAGS, 0x2);
    wreg(cpu->mac_vcpu_fd, HV_X86_RSP, 0x0);
    wreg(cpu->mac_vcpu_fd, HV_X86_RAX, 0x0);
    wreg(cpu->mac_vcpu_fd, HV_X86_RBX, 0x0);
    wreg(cpu->mac_vcpu_fd, HV_X86_RCX, 0x0);
    wreg(cpu->mac_vcpu_fd, HV_X86_RSI, 0x0);
    wreg(cpu->mac_vcpu_fd, HV_X86_RDI, 0x0);
    wreg(cpu->mac_vcpu_fd, HV_X86_RBP, 0x0);

    for (int i = 0; i < 8; i++)
         wreg(cpu->mac_vcpu_fd, HV_X86_R8+i, 0x0);
    memset(cpu->apic_page, 0, 4096);

    cpu->init_tsc = rdtscp();

    if (!osx_is_sierra())
        wvmcs(cpu->mac_vcpu_fd, VMCS_TSC_OFFSET, -cpu->init_tsc);

    hv_vm_sync_tsc(0);
    cpu->hlt = 0;
    hv_vcpu_invalidate_tlb(cpu->mac_vcpu_fd);
    hv_vcpu_flush(cpu->mac_vcpu_fd);
}

int veertu_vcpu_init(CPUState *cpu)
{
    X86CPU *x86cpu;
    int r;

    init_emu(cpu);
    init_decoder(cpu);
    init_cpuid(cpu);

    if (g_hypervisor_iface)
        g_hypervisor_iface->init_cpu_context(cpu);

    r = hv_vcpu_create(&cpu->mac_vcpu_fd, HV_VCPU_DEFAULT);
    if (r) {
        printf("create vcpu failed\n");
        return r;
    }

    cpu->vmx_vcpu_dirty = 1;

	if (hv_vmx_read_capability(HV_VMX_CAP_PINBASED, &cpu->vmx_cap_pinbased)) {
		abort();
	}
	if (hv_vmx_read_capability(HV_VMX_CAP_PROCBASED, &cpu->vmx_cap_procbased)) {
		abort();
	}
	if (hv_vmx_read_capability(HV_VMX_CAP_PROCBASED2, &cpu->vmx_cap_procbased2)) {
		abort();
	}
	if (hv_vmx_read_capability(HV_VMX_CAP_ENTRY, &cpu->vmx_cap_entry)) {
		abort();
	}

	/* set VMCS control fields */
    wvmcs(cpu->mac_vcpu_fd, VMCS_PIN_BASED_CTLS, cap2ctrl(cpu->vmx_cap_pinbased, 0));
    wvmcs(cpu->mac_vcpu_fd, VMCS_PRI_PROC_BASED_CTLS, cap2ctrl(cpu->vmx_cap_procbased,
                                                   VMCS_PRI_PROC_BASED_CTLS_HLT |
                                                   VMCS_PRI_PROC_BASED_CTLS_MWAIT |
                                                   VMCS_PRI_PROC_BASED_CTLS_TSC_OFFSET |
                                                   VMCS_PRI_PROC_BASED_CTLS_TPR_SHADOW) |
                                                   VMCS_PRI_PROC_BASED_CTLS_SEC_CONTROL);
	wvmcs(cpu->mac_vcpu_fd, VMCS_SEC_PROC_BASED_CTLS, cap2ctrl(cpu->vmx_cap_procbased2,VMCS_PRI_PROC_BASED2_CTLS_APIC_ACCESSES));

	wvmcs(cpu->mac_vcpu_fd, VMCS_ENTRY_CTLS, cap2ctrl(cpu->vmx_cap_entry, 0));
	wvmcs(cpu->mac_vcpu_fd, VMCS_EXCEPTION_BITMAP, 0); /* Double fault */

    wvmcs(cpu->mac_vcpu_fd, VMCS_TPR_THRESHOLD, 0);
    addr_t apic_gpa = 0xfee00000;
    if (!cpu->apic_page) {
        posix_memalign(&cpu->apic_page, 4096, 4096);
        memset(cpu->apic_page, 0, 4096);
        hv_vm_map((hv_uvaddr_t)cpu->apic_page, apic_gpa, 4096, HV_MEMORY_READ | HV_MEMORY_WRITE);
        hv_vmx_vcpu_set_apic_address(cpu->mac_vcpu_fd, apic_gpa);
    }

    vmx_reset_vcpu(cpu);
    
    x86cpu = X86_CPU(cpu);
    x86cpu->env.kvm_xsave_buf = vmx_memalign(4096, 4096); // TODO - BUG: bug need to calculate right val
    
    hv_vcpu_enable_native_msr(cpu->mac_vcpu_fd, MSR_STAR, 1);
    hv_vcpu_enable_native_msr(cpu->mac_vcpu_fd, MSR_LSTAR, 1);
    hv_vcpu_enable_native_msr(cpu->mac_vcpu_fd, MSR_CSTAR, 1);
    hv_vcpu_enable_native_msr(cpu->mac_vcpu_fd, MSR_FMASK, 1);
    hv_vcpu_enable_native_msr(cpu->mac_vcpu_fd, MSR_FSBASE, 1);
    hv_vcpu_enable_native_msr(cpu->mac_vcpu_fd, MSR_GSBASE, 1);
    hv_vcpu_enable_native_msr(cpu->mac_vcpu_fd, MSR_KERNELGSBASE, 1);
    hv_vcpu_enable_native_msr(cpu->mac_vcpu_fd, MSR_TSC_AUX, 1);
    //hv_vcpu_enable_native_msr(cpu->mac_vcpu_fd, MSR_IA32_TSC, 1);
    hv_vcpu_enable_native_msr(cpu->mac_vcpu_fd, MSR_IA32_SYSENTER_CS, 1);
    hv_vcpu_enable_native_msr(cpu->mac_vcpu_fd, MSR_IA32_SYSENTER_EIP, 1);
    hv_vcpu_enable_native_msr(cpu->mac_vcpu_fd, MSR_IA32_SYSENTER_ESP, 1);

    return 0;
}

void veertu_set_memory(MemAreaSection *section, int mem_add)
{
    VeertuSlot *mem;
    VeertuMemArea *area = section->mr;
    
    if (!mem_area_is_ram(area))
        return;
    mem = veertu_find_overlap_slot(section->offset_within_address_space,
                                      section->offset_within_address_space + section->size);
    if (mem && mem_add) {
        if (mem->size == section->size &&
            mem->start == section->offset_within_address_space &&
            mem->mem == (memory_area_get_ram_ptr(area) + section->offset_within_region))
            return;
    }
    
    if (mem) {
        mem->size = 0;
        if (__veertu_set_memory(mem)) {
            printf("error register memory\n");
            abort();
        }
    }

    if (mem_add) {
        int x;
        
        for (x = 0; x < veertu_state->num_slots; ++x) {
            mem = &veertu_state->slots[x];
            if (!mem->size)
                break;
        }
        if (x == veertu_state->num_slots) {
            printf("no free slots\n");
            abort();
        }
        mem->size = section->size;
        mem->mem = memory_area_get_ram_ptr(area) + section->offset_within_region;
        mem->start = section->offset_within_address_space;
        if (__veertu_set_memory(mem)) {
            printf("error register memory\n");
            abort();
        }
    }
}

void veertu_region_del(MemoryCallbacks *listener, MemAreaSection *section)
{
    veertu_set_memory(section, false);
}

void veertu_region_add(MemoryCallbacks *listener, MemAreaSection *section)
{
   veertu_set_memory(section, true);
}

void veertu_interrupt_handle(CPUState *cpu_state, int mask)
{
    cpu_state->interrupt_request |= mask;
    if (!vmx_cpu_is_self(cpu_state))
        vmx_cpu_kick(cpu_state);
}

MemoryCallbacks veertu_mem_listener = {
    .priority = 10,
    .region_add = veertu_region_add,
    .region_del = veertu_region_del,
};

MemoryCallbacks veertu_io_listener = {
    .priority = 10,
};

CPUInterruptHandler cpu_interrupt_handler;

int veertu_machine_init(MachineState *machine)
{
    int x;
    int r;

    init_hyperv_iface();

    VeertuState *state = machine->accelerator;
    
    state->num_slots = 32;
    for (x = 0; x < state->num_slots; ++x) {
        state->slots[x].size = 0;
        state->slots[x].slot_id = x;
    }
    
    r = hv_vm_create(HV_VM_DEFAULT);
    if (r) {
        printf("Error creating hv_vm_create\n");
        return r;
    }
    
    veertu_state = state;
    cpu_interrupt_handler = veertu_interrupt_handle;
    memory_callbacks_register(&veertu_mem_listener, &address_space_memory);
    memory_callbacks_register(&veertu_io_listener, &address_space_io);
    
    return 0;
}

extern void vmx_fs_port_write(hwaddr addr, uint64_t val, unsigned size);
extern uint32_t vmx_fs_port_read(hwaddr addr, unsigned size);

void veertu_handle_io(CPUState *cpu_state, uint16_t port, void *data, int direction, int size, uint32_t count)
{
    int x;
    uint8_t *ptr = data;

    if (port == 0x185c) {
        if (direction) {
            vmx_fs_port_write(3, *(uint32_t *)data, size);
        } else {
            *((uint32_t *)data) = vmx_fs_port_read(3, size);
        }
        return;
    }
    
    if (port == 0x1854) {
        if (direction) {
            vmx_fs_port_write(1, *(uint32_t *)data, size);
        } else {
            *((uint32_t *)data) = vmx_fs_port_read(1, size);
        }
        return;
    }
    
    if (port == 0x1850) {
        if (direction) {
            vmx_fs_port_write(0, *(uint32_t *)data, size);
        } else {
            *((uint32_t *)data) = vmx_fs_port_read(0, size);
        }
        return;
    }
    
    if (port == 0x1858) {
        if (direction) {
            vmx_fs_port_write(2, *(uint32_t *)data, size);
        } else {
            *((uint32_t *)data) = vmx_fs_port_read(2, size);
        }
        return;
    }

    for (x = 0; x < count ;++x) {
        address_space_rw(&address_space_io, port, ptr, size, direction);
        ptr += size;
    }
}

void __veertu_cpu_synchronize_state(void *data)
{
    CPUState *cpu_state = (CPUState *)data;
    if (cpu_state->vmx_vcpu_dirty == 0)
        veertu_get_registers(cpu_state);

    cpu_state->vmx_vcpu_dirty = 1;
}

void veertu_cpu_synchronize_state(CPUState *cpu_state)
{
    if (cpu_state->vmx_vcpu_dirty == 0)
        run_on_cpu(cpu_state, __veertu_cpu_synchronize_state, cpu_state);
}



void __veertu_cpu_synchronize_post_reset(void *data)
{
    CPUState *cpu_state = (CPUState *)data;
    veertu_put_registers(cpu_state, VEERTU_PUT_RESET_STATE);
    cpu_state->vmx_vcpu_dirty = false;
}

void veertu_cpu_synchronize_post_reset(CPUState *cpu_state)
{
    run_on_cpu(cpu_state, __veertu_cpu_synchronize_post_reset, cpu_state);
}

void _veertu_cpu_synchronize_post_init(void *data)
{
    CPUState *cpu_state = (CPUState *)data;
    veertu_put_registers(cpu_state, VEERTU_PUT_FULL_STATE);
    cpu_state->vmx_vcpu_dirty = false;
}

void veertu_cpu_synchronize_post_init(CPUState *cpu_state)
{
    run_on_cpu(cpu_state, _veertu_cpu_synchronize_post_init, cpu_state);
}

void veertu_cpu_clean_state(CPUState *cpu_state)
{
    cpu_state->vmx_vcpu_dirty = 0;
}

void vmx_clear_int_window_exiting(CPUState *cpu);

static bool ept_emulation_fault(uint64_t ept_qual)
{
	int read, write;

	/* EPT fault on an instruction fetch doesn't make sense here */
	if (ept_qual & EPT_VIOLATION_INST_FETCH)
		return false;

	/* EPT fault must be a read fault or a write fault */
	read = ept_qual & EPT_VIOLATION_DATA_READ ? 1 : 0;
	write = ept_qual & EPT_VIOLATION_DATA_WRITE ? 1 : 0;
	if ((read | write) == 0)
		return false;

	/*
	 * The EPT violation must have been caused by accessing a
	 * guest-physical address that is a translation of a guest-linear
	 * address.
	 */
	if ((ept_qual & EPT_VIOLATION_GLA_VALID) == 0 ||
	    (ept_qual & EPT_VIOLATION_XLAT_VALID) == 0) {
		return false;
	}

	return true;
}

static void save_state_to_tss32(CPUState *cpu, struct x86_tss_segment32 *tss)
{
    /* CR3 and ldt selector are not saved intentionally */
    tss->eip = EIP(cpu);
    tss->eflags = EFLAGS(cpu);
    tss->eax = EAX(cpu);
    tss->ecx = ECX(cpu);
    tss->edx = EDX(cpu);
    tss->ebx = EBX(cpu);
    tss->esp = ESP(cpu);
    tss->ebp = EBP(cpu);
    tss->esi = ESI(cpu);
    tss->edi = EDI(cpu);

    tss->es = vmx_read_segment_selector(cpu, REG_SEG_ES).sel;
    tss->cs = vmx_read_segment_selector(cpu, REG_SEG_CS).sel;
    tss->ss = vmx_read_segment_selector(cpu, REG_SEG_SS).sel;
    tss->ds = vmx_read_segment_selector(cpu, REG_SEG_DS).sel;
    tss->fs = vmx_read_segment_selector(cpu, REG_SEG_FS).sel;
    tss->gs = vmx_read_segment_selector(cpu, REG_SEG_GS).sel;
}

static void load_state_from_tss32(CPUState *cpu, struct x86_tss_segment32 *tss)
{
    wvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_CR3, tss->cr3);

    RIP(cpu) = tss->eip;
    EFLAGS(cpu) = tss->eflags | 2;

    /* General purpose registers */
    RAX(cpu) = tss->eax;
    RCX(cpu) = tss->ecx;
    RDX(cpu) = tss->edx;
    RBX(cpu) = tss->ebx;
    RSP(cpu) = tss->esp;
    RBP(cpu) = tss->ebp;
    RSI(cpu) = tss->esi;
    RDI(cpu) = tss->edi;

    vmx_write_segment_selector(cpu, (x68_segment_selector){tss->ldt}, REG_SEG_LDTR);
    vmx_write_segment_selector(cpu, (x68_segment_selector){tss->es}, REG_SEG_ES);
    vmx_write_segment_selector(cpu, (x68_segment_selector){tss->cs}, REG_SEG_CS);
    vmx_write_segment_selector(cpu, (x68_segment_selector){tss->ss}, REG_SEG_SS);
    vmx_write_segment_selector(cpu, (x68_segment_selector){tss->ds}, REG_SEG_DS);
    vmx_write_segment_selector(cpu, (x68_segment_selector){tss->fs}, REG_SEG_FS);
    vmx_write_segment_selector(cpu, (x68_segment_selector){tss->gs}, REG_SEG_GS);

#if 0
    load_segment(cpu, REG_SEG_LDTR, tss->ldt);
    load_segment(cpu, REG_SEG_ES, tss->es);
    load_segment(cpu, REG_SEG_CS, tss->cs);
    load_segment(cpu, REG_SEG_SS, tss->ss);
    load_segment(cpu, REG_SEG_DS, tss->ds);
    load_segment(cpu, REG_SEG_FS, tss->fs);
    load_segment(cpu, REG_SEG_GS, tss->gs);
#endif
}

static int task_switch_32(CPUState *cpu, x68_segment_selector tss_sel, x68_segment_selector old_tss_sel,
                          uint64_t old_tss_base, struct x86_segment_descriptor *new_desc)
{
    struct x86_tss_segment32 tss_seg;
    uint32_t new_tss_base = x86_segment_base(new_desc);
    uint32_t eip_offset = offsetof(struct x86_tss_segment32, eip);
    uint32_t ldt_sel_offset = offsetof(struct x86_tss_segment32, ldt);

    vmx_read_mem(cpu, &tss_seg, old_tss_base, sizeof(tss_seg));
    save_state_to_tss32(cpu, &tss_seg);

    vmx_write_mem(cpu, old_tss_base + eip_offset, &tss_seg.eip, ldt_sel_offset - eip_offset);
    vmx_read_mem(cpu, &tss_seg, new_tss_base, sizeof(tss_seg));

    if (old_tss_sel.sel != 0xffff) {
        tss_seg.prev_tss = old_tss_sel.sel;

        vmx_write_mem(cpu, new_tss_base, &tss_seg.prev_tss, sizeof(tss_seg.prev_tss));
    }
    load_state_from_tss32(cpu, &tss_seg);
    return 0;
}

static void vmx_handle_task_switch(CPUState *cpu, x68_segment_selector tss_sel, int reason, bool gate_valid, uint8_t gate, uint64_t gate_type)
{
    uint64_t rip = rreg(cpu->mac_vcpu_fd, HV_X86_RIP);
    if (!gate_valid || (gate_type != VMCS_INTR_T_HWEXCEPTION &&
                        gate_type != VMCS_INTR_T_HWINTR &&
                        gate_type != VMCS_INTR_T_NMI)) {
        int ins_len = rvmcs(cpu->mac_vcpu_fd, VMCS_EXIT_INSTRUCTION_LENGTH);
        macvm_set_rip(cpu, rip + ins_len);
        return;
    }

    load_regs(cpu);

    struct x86_segment_descriptor curr_tss_desc, next_tss_desc;
    int ret;
    x68_segment_selector old_tss_sel = vmx_read_segment_selector(cpu, REG_SEG_TR);
    uint64_t old_tss_base = vmx_read_segment_base(cpu, REG_SEG_TR);
    uint32_t desc_limit;
    struct x86_call_gate task_gate_desc;
    struct vmx_segment vmx_seg;

    x86_read_segment_descriptor(cpu, &next_tss_desc, tss_sel);
    x86_read_segment_descriptor(cpu, &curr_tss_desc, old_tss_sel);

    if (reason == TSR_IDT_GATE && gate_valid) {
        int dpl;

        ret = x86_read_call_gate(cpu, &task_gate_desc, gate);

        dpl = task_gate_desc.dpl;
        x68_segment_selector cs = vmx_read_segment_selector(cpu, REG_SEG_CS);
        if (tss_sel.rpl > dpl || cs.rpl > dpl)
            printf("emulate_gp");
    }

    desc_limit = x86_segment_limit(&next_tss_desc);
    if (!next_tss_desc.p || ((desc_limit < 0x67 && (next_tss_desc.type & 8)) || desc_limit < 0x2b)) {
        VM_PANIC("emulate_ts");
    }

    if (reason == TSR_IRET || reason == TSR_JMP) {
        curr_tss_desc.type &= ~(1 << 1); /* clear busy flag */
        x86_write_segment_descriptor(cpu, &curr_tss_desc, old_tss_sel);
    }

    if (reason == TSR_IRET)
        EFLAGS(cpu) &= ~RFLAGS_NT;

    if (reason != TSR_CALL && reason != TSR_IDT_GATE)
        old_tss_sel.sel = 0xffff;

    if (reason != TSR_IRET) {
        next_tss_desc.type |= (1 << 1); /* set busy flag */
        x86_write_segment_descriptor(cpu, &next_tss_desc, tss_sel);
    }

    if (next_tss_desc.type & 8)
        ret = task_switch_32(cpu, tss_sel, old_tss_sel, old_tss_base, &next_tss_desc);
    else
        //ret = task_switch_16(cpu, tss_sel, old_tss_sel, old_tss_base, &next_tss_desc);
        VM_PANIC("task_switch_16");

    macvm_set_cr0(cpu->mac_vcpu_fd, rvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_CR0) | CR0_TS);
    x86_segment_descriptor_to_vmx(cpu, tss_sel, &next_tss_desc, &vmx_seg);
    vmx_write_segment_descriptor(cpu, &vmx_seg, REG_SEG_TR);

    store_regs(cpu);

    hv_vcpu_invalidate_tlb(cpu->mac_vcpu_fd);
    hv_vcpu_flush(cpu->mac_vcpu_fd);
}

void vmx_update_tpr(CPUState *cpu)
{
    X86CPU *x86_cpu = X86_CPU(cpu);
    int tpr = cpu_get_apic_tpr(x86_cpu->apic_state) << 4;
    int irr = apic_get_highest_priority_irr(x86_cpu->apic_state);

    wreg(cpu->mac_vcpu_fd, HV_X86_TPR, tpr);
    if (irr == -1)
        wvmcs(cpu->mac_vcpu_fd, VMCS_TPR_THRESHOLD, 0);
    else
        wvmcs(cpu->mac_vcpu_fd, VMCS_TPR_THRESHOLD, (irr > tpr) ? tpr >> 4 : irr >> 4);
}

void update_apic_tpr(CPUState *cpu)
{
    X86CPU *x86_cpu = X86_CPU(cpu);
    int tpr = rreg(cpu->mac_vcpu_fd, HV_X86_TPR) >> 4;
    cpu_set_apic_tpr(x86_cpu->apic_state, tpr);
}

#define VECTORING_INFO_VECTOR_MASK     0xff

int veertu_cpu_exec(CPUState *cpu)
{
    X86CPU *x86_cpu = X86_CPU(cpu);
    CPUX86State *env = &x86_cpu->env;
    int ret = 0;
    uint64_t rip = 0;

    if (veertu_process_events(cpu)) {
        vmx_mutex_unlock_iothread();
        pthread_yield_np();
        vmx_mutex_lock_iothread();
        return EXCP_HLT;
    }

again:
    do {
        if (cpu->vmx_vcpu_dirty) {
            veertu_put_registers(cpu, VEERTU_PUT_RUNTIME_STATE);
            cpu->vmx_vcpu_dirty = false;
        }
        
        cpu->interruptable = !(rvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_INTERRUPTIBILITY) &
                               (VMCS_INTERRUPTIBILITY_STI_BLOCKING | VMCS_INTERRUPTIBILITY_MOVSS_BLOCKING));
        
        veertu_inject_interrupts(cpu);
        vmx_update_tpr(cpu);
        
        vmx_mutex_unlock_iothread();
        
        while (!cpu_is_bsp(X86_CPU(cpu)) && cpu->halted) {
            vmx_mutex_lock_iothread();
            return EXCP_HLT;
        }
        
        int r;
        if ((r = hv_vcpu_run(cpu->mac_vcpu_fd))) {
            printf("%ld: run %llx failed with %x\n", veertu_vcpu_id(cpu), rip, r);
            abort();
        }

        /* handle VMEXIT */
        uint64_t exit_reason = rvmcs(cpu->mac_vcpu_fd, VMCS_EXIT_REASON);
        uint64_t exit_qual = rvmcs(cpu->mac_vcpu_fd, VMCS_EXIT_QUALIFICATION);
        uint32_t ins_len = (uint32_t)rvmcs(cpu->mac_vcpu_fd, VMCS_EXIT_INSTRUCTION_LENGTH);
        uint64_t idtvec_info = rvmcs(cpu->mac_vcpu_fd, VMCS_IDT_VECTORING_INFO);
        rip = rreg(cpu->mac_vcpu_fd, HV_X86_RIP);
        RFLAGS(cpu) = rreg(cpu->mac_vcpu_fd, HV_X86_RFLAGS);
        env->eflags = RFLAGS(cpu);

        vmx_mutex_lock_iothread();
        
        update_apic_tpr(cpu);
        current_cpu = cpu;
        
        ret = 0;
        switch (exit_reason) {
            case EXIT_REASON_HLT: {
                macvm_set_rip(cpu, rip + ins_len);
                if (!((cpu->interrupt_request & CPU_INTERRUPT_HARD) && (EFLAGS(cpu) & IF_MASK)) && !(cpu->interrupt_request & CPU_INTERRUPT_NMI) && !(idtvec_info & VMCS_IDT_VEC_VALID)) {
                    cpu->hlt = 1;
                    ret = EXCP_HLT;
                }
                ret = EXCP_INTERRUPT;
                break;
            }
            case EXIT_REASON_MWAIT: {
                ret = EXCP_INTERRUPT;
                break;
            }
                /* Need to check if MMIO or unmmaped fault */
            case EXIT_REASON_EPT_FAULT:
            {
                VeertuSlot *slot;
                addr_t gpa = rvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_PHYSICAL_ADDRESS);
                
                if ((idtvec_info & VMCS_IDT_VEC_VALID) == 0 && (exit_qual & EXIT_QUAL_NMIUDTI) != 0)
                    vmx_set_nmi_blocking(cpu);
                
                slot = veertu_find_overlap_slot(gpa, gpa);
                // mmio
                if (ept_emulation_fault(exit_qual) && !slot) {
                    struct x86_decode decode;
                    
                    load_regs(cpu);
                    cpu->fetch_rip = rip;
                    
                    decode_instruction(cpu, &decode);
#if 0
                    printf("%llx: fetched %s, %x %x modrm %x len %d, gpa %lx\n", rip, decode_cmd_to_string(decode.cmd),
                           decode.opcode[0], decode.opcode[1], decode.modrm.byte, decode.len, gpa);
#endif
                    exec_instruction(cpu, &decode);
                    store_regs(cpu);
                    break;
                }
#ifdef DIRTY_VGA_TRACKING
                if (slot) {
                    bool read = exit_qual & EPT_VIOLATION_DATA_READ ? 1 : 0;
                    bool write = exit_qual & EPT_VIOLATION_DATA_WRITE ? 1 : 0;
                    if (!read && !write)
                        break;
                    int flags = HV_MEMORY_READ | HV_MEMORY_EXEC;
                    if (write) flags |= HV_MEMORY_WRITE;
                    
                    pthread_rwlock_wrlock(&mem_lock);
                    if (write)
                        mark_slot_page_dirty(slot, gpa);
                    hv_vm_protect(gpa & ~0xfff, 4096, flags);
                    pthread_rwlock_unlock(&mem_lock);
                }
#endif
                break;
            }
            case EXIT_REASON_INOUT:
            {
                uint32_t in = (exit_qual & 8) != 0;
                uint32_t size =  (exit_qual & 7) + 1;
                uint32_t string =  (exit_qual & 16) != 0;
                uint32_t port =  exit_qual >> 16;
                uint32_t rep = (exit_qual & 0x20) != 0;
                
#if 1
                if (!string && in) {
                    uint64_t val = 0;
                    load_regs(cpu);
                    veertu_handle_io(cpu, port, &val, 0, size, 1);
                    if (size == 1) AL(cpu) = val;
                    else if (size == 2) AX(cpu) = val;
                    else if (size == 4) RAX(cpu) = (uint32_t)val;
                    else VM_PANIC("size");
                    RIP(cpu) += ins_len;
                    store_regs(cpu);
                    break;
                } else if (!string && !in) {
                    RAX(cpu) = rreg(cpu->mac_vcpu_fd, HV_X86_RAX);
                    veertu_handle_io(cpu, port, &RAX(cpu), 1, size, 1);
                    macvm_set_rip(cpu, rip + ins_len);
                    break;
                }
#endif
                struct x86_decode decode;
                
                load_regs(cpu);
                cpu->fetch_rip = rip;
                
                decode_instruction(cpu, &decode);
                //printf("%llx: IN/OUT fetched %s, %x %x len %d\n", rip, decode_cmd_to_string(decode.cmd), decode.opcode[0], decode.opcode[1], decode.len);
                VM_PANIC_ON(ins_len != decode.len);
                exec_instruction(cpu, &decode);
                store_regs(cpu);
                
                break;
            }
            case EXIT_REASON_CPUID: {
                uint32_t rax = (uint32_t)rreg(cpu->mac_vcpu_fd, HV_X86_RAX);
                uint32_t rbx = (uint32_t)rreg(cpu->mac_vcpu_fd, HV_X86_RBX);
                uint32_t rcx = (uint32_t)rreg(cpu->mac_vcpu_fd, HV_X86_RCX);
                uint32_t rdx = (uint32_t)rreg(cpu->mac_vcpu_fd, HV_X86_RDX);
                
                get_cpuid_func(cpu, rax, rcx, &rax, &rbx, &rcx, &rdx);
                
                wreg(cpu->mac_vcpu_fd, HV_X86_RAX, rax);
                wreg(cpu->mac_vcpu_fd, HV_X86_RBX, rbx);
                wreg(cpu->mac_vcpu_fd, HV_X86_RCX, rcx);
                wreg(cpu->mac_vcpu_fd, HV_X86_RDX, rdx);
                
                // printf("cpuid %lx at %lx\n", rax, rip);
                macvm_set_rip(cpu, rip + ins_len);
                break;
            }
            case EXIT_REASON_XSETBV: {
                X86CPU *x86_cpu = X86_CPU(cpu);
                CPUX86State *env = &x86_cpu->env;
                uint32_t eax = (uint32_t)rreg(cpu->mac_vcpu_fd, HV_X86_RAX);
                uint32_t ecx = (uint32_t)rreg(cpu->mac_vcpu_fd, HV_X86_RCX);
                uint32_t edx = (uint32_t)rreg(cpu->mac_vcpu_fd, HV_X86_RDX);
                
                if (ecx) {
                    printf("xsetbv: invalid index %d\n", ecx);
                    macvm_set_rip(cpu, rip + ins_len);
                    break;
                }
                env->xcr0 = ((uint64_t)edx << 32) | eax;
                wreg(cpu->mac_vcpu_fd, HV_X86_XCR0, env->xcr0 | 1);
                macvm_set_rip(cpu, rip + ins_len);
                break;
            }
            case EXIT_REASON_INTR_WINDOW:
                vmx_clear_int_window_exiting(cpu);
                ret = EXCP_INTERRUPT;
                break;
            case EXIT_REASON_NMI_WINDOW:
                vmx_clear_nmi_window_exiting(cpu);
                ret = EXCP_INTERRUPT;
                break;
            case EXIT_REASON_EXT_INTR:
                /* force exit and allow io handling */
                ret = EXCP_INTERRUPT;
                break;
            case EXIT_REASON_RDMSR:
            case EXIT_REASON_WRMSR:
            {
                load_regs(cpu);
                if (exit_reason == EXIT_REASON_RDMSR)
                    simulate_rdmsr(cpu);
                else
                    simulate_wrmsr(cpu);
                RIP(cpu) += rvmcs(cpu->mac_vcpu_fd, VMCS_EXIT_INSTRUCTION_LENGTH); 
                store_regs(cpu);
                break;
            }
            case EXIT_REASON_CR_ACCESS: {
                int cr;
                int reg;
                
                load_regs(cpu);
                cr = exit_qual & 15;
                reg = (exit_qual >> 8) & 15;
                
                //printf("%lx: mov cr %d from %d %llx\n", rip, cr, reg, RXX(cpu, reg));
                switch (cr) {
                    case 0x0: {
                        macvm_set_cr0(cpu->mac_vcpu_fd, RRX(cpu, reg));
                        break;
                    }
                    case 4: {
                        macvm_set_cr4(cpu->mac_vcpu_fd, RRX(cpu, reg));
                        break;
                    }
                    case 8: {
                        X86CPU *x86_cpu = X86_CPU(cpu);
                        if (exit_qual & 0x10) {
                            RRX(cpu, reg) = cpu_get_apic_tpr(x86_cpu->apic_state);
                        }
                        else {
                            int tpr = RRX(cpu, reg);
                            cpu_set_apic_tpr(x86_cpu->apic_state, tpr);
                            ret = EXCP_INTERRUPT;
                        }
                        break;
                    }
                    default:
                        abort();
                }
                RIP(cpu) += ins_len;
                store_regs(cpu);
                break;
            }
            case EXIT_REASON_APIC_ACCESS: {
                struct x86_decode decode;
                
                load_regs(cpu);
                cpu->fetch_rip = rip;
                
                decode_instruction(cpu, &decode);
                //printf("apic fetched %s, %x %x len %d\n", decode_cmd_to_string(decode.cmd), decode.opcode[0], decode.opcode[1], decode.len);
                exec_instruction(cpu, &decode);
                store_regs(cpu);
                break;
            }
            case EXIT_REASON_TPR: {
                ret = 1;
                break;
            }
            case EXIT_REASON_TASK_SWITCH: {
                uint64_t vinfo = rvmcs(cpu->mac_vcpu_fd, VMCS_IDT_VECTORING_INFO);
                printf("%llx: task switch %lld, vector %lld, gate %lld\n", RIP(cpu), (exit_qual >> 30) & 0x3, vinfo & VECTORING_INFO_VECTOR_MASK, vinfo & VMCS_INTR_T_MASK);
                x68_segment_selector sel = {.sel = exit_qual & 0xffff};
                vmx_handle_task_switch(cpu, sel, (exit_qual >> 30) & 0x3, vinfo & VMCS_INTR_VALID, vinfo & VECTORING_INFO_VECTOR_MASK, vinfo & VMCS_INTR_T_MASK);
                break;
            }
            case EXIT_REASON_TRIPLE_FAULT: {
                addr_t gpa = rvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_PHYSICAL_ADDRESS);
                
                printf("triple fault at %llx (%llx, %llx), cr0 %llx, qual %llx, gpa %llx, ins len %d, cpu %p\n", linear_rip(cpu, rip), RIP(cpu), linear_addr(cpu, rip, REG_SEG_CS),
                       rvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_CR0), exit_qual, gpa, ins_len, cpu);
                vmx_system_reset_request();
                usleep(1000 * 100);
                ret = EXCP_INTERRUPT;
                break;
            }
            case EXIT_REASON_RDPMC:
                wreg(cpu->mac_vcpu_fd, HV_X86_RAX, 0);
                wreg(cpu->mac_vcpu_fd, HV_X86_RDX, 0);
                macvm_set_rip(cpu, rip + ins_len);
                break;
            case VMX_REASON_VMCALL:
                if (g_hypervisor_iface) {
                    load_regs(cpu);
                    g_hypervisor_iface->hypercall_handler(cpu);
                    RIP(cpu) += rvmcs(cpu->mac_vcpu_fd, VMCS_EXIT_INSTRUCTION_LENGTH); 
                    store_regs(cpu);
                }
                break;
            default:
                VM_PANIC_EX("%llx: unhandled exit %llx, cpu id %ld\n", rip, exit_reason, veertu_vcpu_id(cpu));
        }
    } while (ret == 0);
    
    return ret;
}

bool veertu_allowed;

void veertu_accel_init(VeertuTypeClassHold *objectclass, void *data)
{
    AccelClass *accel = ACCEL_CLASS(objectclass);
    accel->name = "VEERTU";
    accel->allowed = &veertu_allowed;
    accel->init_machine = veertu_machine_init;
}

VeertuTypeInfo veertu_accel = {
    .instance_size = sizeof(VeertuState),
    .name = ACCEL_CLASS_NAME("vmx"),
    .parent = TYPE_ACCEL,
    .class_init = veertu_accel_init,
};

void veertu_init_types()
{
    register_type_internal(&veertu_accel);
}

type_init(veertu_init_types);

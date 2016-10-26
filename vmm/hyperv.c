
#include "hyperv.h"
#include "vmm/x86.h"
#include "vmx.h"
#include "iapic.h"
#include "qemu/timer.h"
#include "known_hypervisor_interface.h"
#include "vmstate.h"
#include <sys/types.h>
#include <sys/sysctl.h>

struct known_hypervisor_interface* g_hypervisor_iface = NULL;

// "0F 01 C1" is the encoding of the VMCALL instruction (Intel VT-x)
// "C3" is the encoding of RET instruction
static const uint8_t hypercall_code_bytes[] = {0x0f, 0x01, 0xc1, 0xc3};

static void hyperv_init_cpu_context(struct CPUState* cpu)
{
    cpu->hypervisor_iface_context = g_malloc0(sizeof(struct hyperv_cpu_context));
    if (NULL == cpu->hypervisor_iface_context) {
        printf("Failed to allocate memory for hypervisor_cpu_context\n");
        abort();
    }
}

static void hyperv_cpuid_handler(
        struct CPUState* cpu, 
        int func, int cnt, 
        uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx)
{
    struct hyperv_interface* hyperv_iface = (struct hyperv_interface*)g_hypervisor_iface;

    switch (func) {
        case HYPERV_ID:
            *eax = HYPERV_IMPLEMENTATION_HW_FEATURES;   /* Max supported Hyper-V cpuid leaf */
            *ebx = 'rciM';
            *ecx = 'foso';
            *edx = 'vH t';
            break;

        case HYPERV_VENDOR_NEUTRAL_ID:
            *eax = '1#vH';
            *ebx = 0;
            *ecx = 0;
            *edx = 0;
            break;

        case HYPERV_SYSTEM_ID:
            /* Return 0 in all fields until guest OS will specify it's version */
            if (hyperv_iface->guest_os_version.qword == 0) {
                *eax = 0;
                *ebx = 0;
                *ecx = 0;
                *edx = 0;
            } else {
                /* Note - numbers were taken from KVM's Hyper-V support (target-i386/kvm.c in QEMU src) */
                *eax = 7100;        /* Build number */
                *ebx = 0x60001;     /* Bits 16-31: Major Version
                                       Bits 0-15: Minor Version */
                *ecx = 0;           /* Service Pack */
                *edx = 0;           /* Bits 24-31: Service Branch
                                       Bits 0-23: Service Number */
            }
            break;

        case HYPERV_FEATURE_ID:
            /* eax indicates which features are available to the partition based upon the current partition priviledges */
            *eax = CPUID_AccessApicMsrs | CPUID_AccessHypercallMsrs | CPUID_AccessVpIndex;
            /* ebx indicates which flags were specified at partition creation */
            *ebx = 0;
            /* ecx contains power management related information
             * Bits 0-3: Max processor power state (C0-C3)
             * Bit 4: HPET is required to enter C3
             * Bits 5-31: reserved */
            *ecx = 0;   /* We don't allow the guest to be at another power state than C0 */
            /* edx indicates which misc features are avilable to the partition */
            *edx = 0;
            break;

        case HYPERV_ENLIGHTMENT_RECOMMENDATIONS:
            /* eax indicates which behaviours the hypervisor recommends the OS implement for optimal performance */
            *eax = CPUID_UseRelaxedTimingInCurrentParrtition | CPUID_UseMSRsForAPICAccess;
            /* ebx indicates recommended number of attempts to retry a spinlock failure (0 == disable, 0xffffffff == never to retry) */
            *ebx = X86_CPU(cpu)->hyperv_spinlock_attempts;
            /* ecx & edx are reserved */
            break;

        case HYPERV_IMPLEMENTATION_LIMITS:
            /* 0x40 was selected because it is the same value that kvm use */
            *eax = 0x40;    /* The max number of virtual processors supported */
            *ebx = 0x40;    /* The max number of logical processors supported */
            *ecx = 0;       /* The max number of phyusical interrupt vectors available for interrupt remapping */
            /* edx are reserved */
            break;
        case HYPERV_IMPLEMENTATION_HW_FEATURES:
            /* eax indicates implementation recommendations */
            *eax = CPUID_SecondLevelAddressTranslation;
            /* ebx & ecx are resrved */
            /* edx reserved for future AMD-specific features */
            break;
    }
}

static bool is_partition_wide_msr(uint32_t msr)
{
    switch (msr) {
        case HV_X64_MSR_GUEST_OS_ID:
        case HV_X64_MSR_HYPERCALL:
        case HV_X64_MSR_TIME_REFERENCE_TSC:
        case HV_X64_MSR_TIME_REF_COUNT:
        case HV_X64_MSR_CRASH_CTL:
        case HV_X64_MSR_CRASH_P0 ... HV_X64_MSR_CRASH_P4:
            return true;
    }
    
    return false;
}


void vmx_apic_mem_writel(void *opaque, hwaddr addr, uint32_t val);

static void hyperv_wrmsr_handler(
        struct CPUState* cpu,
        uint32_t msr, uint64_t value)
{
    struct hyperv_interface* hyperv_iface = (struct hyperv_interface*)g_hypervisor_iface;
    struct hyperv_cpu_context* hyperv_cpu = (struct hyperv_cpu_context*)cpu->hypervisor_iface_context;
    const bool partition_wide_msr = is_partition_wide_msr(msr);

    if (partition_wide_msr)
        vmx_mutex_lock(&hyperv_iface->partition_lock);
    
    switch (msr) {
        case HV_X64_MSR_GUEST_OS_ID:
            hyperv_iface->guest_os_version.qword = value;
            printf(
                "HV_X64_MSR_GUEST_OS_ID detected Guest OS Version: os_type=%d, vendor_id=%d, os_id=%d, major_version=%d, minor_version=%d, service_version=%d, build_number=%d\n", 
                hyperv_iface->guest_os_version.fields.os_type,
                hyperv_iface->guest_os_version.fields.vendor_id,
                hyperv_iface->guest_os_version.fields.os_id,
                hyperv_iface->guest_os_version.fields.major_version,
                hyperv_iface->guest_os_version.fields.minor_version,
                hyperv_iface->guest_os_version.fields.service_version,
                hyperv_iface->guest_os_version.fields.build_number);

            /* If Guest OS clears version info, then hypercall mechanism is disabled */
            if (!value)
                hyperv_iface->msr_hypercall &= ~1;

            break;

       case HV_X64_MSR_HYPERCALL:
            if (hyperv_iface->guest_os_version.qword == 0)
                break;

            hyperv_iface->msr_hypercall = value;
            /* LSB is enable-bit */
            if (value & 1) {
                uint64_t hypercall_gpa = value & TARGET_PAGE_MASK;
                address_space_rw(&address_space_memory, (uint64_t *)hypercall_gpa, hypercall_code_bytes, sizeof(hypercall_code_bytes), 1);
            }
            break;

        case HV_X64_MSR_TIME_REF_COUNT:
            /* Any attempt to write result in #GP fault */
            break;
            
        case HV_X64_MSR_TIME_REFERENCE_TSC:
            hyperv_iface->msr_reference_tsc = value;
            /* LSB is enable-bit */
            if (value & 1) {
                uint64_t reference_tsc_gpa = value & TARGET_PAGE_MASK;
                address_space_memset(&address_space_memory, (uint64_t *)reference_tsc_gpa, 0, sizeof(struct hv_reference_tsc_page));
            }
            break;
            
        case HV_X64_MSR_CRASH_P0 ... HV_X64_MSR_CRASH_P4:
            hyperv_iface->msr_crash_params[msr - HV_X64_MSR_CRASH_P0] = value;
            break;

        case HV_X64_MSR_CRASH_CTL:
            if (value == HV_MSR_CRASH_CTL_CRASH_NOTIFY) {
                printf(
                    "Guest OS crash detected!!!\n"
                    "Crash Parameters:\n"
                    "Parameter0: 0x%lx\n"
                    "Parameter1: 0x%lx\n"
                    "Parameter2: 0x%lx\n"
                    "Parameter3: 0x%lx\n"
                    "Parameter4: 0x%lx\n", 
                    hyperv_iface->msr_crash_params[0], 
                    hyperv_iface->msr_crash_params[1],
                    hyperv_iface->msr_crash_params[2],
                    hyperv_iface->msr_crash_params[3],
                    hyperv_iface->msr_crash_params[4]);
            }
            break;
            
        case HV_X64_MSR_EOI:
            vmx_apic_mem_writel(X86_CPU(cpu)->apic_state, 0xb0, value);
            break;

        case HV_X64_MSR_ICR:
            vmx_apic_mem_writel(X86_CPU(cpu)->apic_state, 0x310, (uint32_t)(value >> 32));
            vmx_apic_mem_writel(X86_CPU(cpu)->apic_state, 0x300, (uint32_t)value);
            break;

        case HV_X64_MSR_TPR:
            cpu_set_apic_tpr(X86_CPU(cpu)->apic_state, value);
            break;

        case HV_X64_MSR_APIC_ASSIST_PAGE:
            hyperv_cpu->msr_apic_assist = value;
            /* LSB is enable-bit */
            if (value & 1) {
                uint64_t apic_assist_gpa = value & TARGET_PAGE_MASK;
                address_space_memset(&address_space_memory, apic_assist_gpa, 0, TARGET_PAGE_SIZE);
            }
            break;

        default:
            //if ((msr >= HV_X64_MSR_GUEST_OS_ID) && (msr <= HV_MAX_MSR))
            //    printf("WRMSR to unimplemented Hyper-V MSR: 0x%x\n", msr);
            break;
    };

    if (partition_wide_msr)
        vmx_mutex_unlock(&hyperv_iface->partition_lock);
}


static uint64_t hyperv_rdmsr_handler(
    struct CPUState* cpu,
    uint32_t msr)
{
    struct hyperv_interface* hyperv_iface = (struct hyperv_interface*)g_hypervisor_iface;
    struct hyperv_cpu_context* hyperv_cpu = (struct hyperv_cpu_context*)cpu->hypervisor_iface_context;
    const bool partition_wide_msr = is_partition_wide_msr(msr);
    uint64_t value;

    if (partition_wide_msr)
        vmx_mutex_lock(&hyperv_iface->partition_lock);

    switch (msr) {
    
        case HV_X64_MSR_GUEST_OS_ID:
            value = hyperv_iface->guest_os_version.qword;
            RAX(cpu) = (uint32_t)value;
            RDX(cpu) = (uint32_t)(value >> 32);
            break;

        case HV_X64_MSR_HYPERCALL:
            value = hyperv_iface->msr_hypercall;
            RAX(cpu) = (uint32_t)value;
            RDX(cpu) = (uint32_t)(value >> 32);
            break;

        case HV_X64_MSR_VP_INDEX:
            RAX(cpu) = (uint32_t)value;
            RDX(cpu) = (uint32_t)(value >> 32);
            break;

        case HV_X64_MSR_TIME_REFERENCE_TSC:
            value = hyperv_iface->msr_reference_tsc;
            RAX(cpu) = (uint32_t)value;
            RDX(cpu) = (uint32_t)(value >> 32);
            break;
            
        case HV_X64_MSR_TIME_REF_COUNT:
            /*
             * Read the partition counter to get the current tick count. This count
             * is set to 0 when the partition is created and is incremented in
             * 100 nanosecond units.
             */
            value = (vmx_clock_get_ns(QEMU_CLOCK_REALTIME) - hyperv_iface->rtclock_offset) / 100;
            RAX(cpu) = (uint32_t)value;
            RDX(cpu) = (uint32_t)(value >> 32);
            break;

        case HV_X64_MSR_CRASH_CTL:
            value = HV_MSR_CRASH_CTL_CRASH_NOTIFY;
            RAX(cpu) = (uint32_t)value;
            RDX(cpu) = (uint32_t)(value >> 32);
            break;

        case HV_X64_MSR_EOI:
            printf("eoi\n");
            break;

        case HV_X64_MSR_ICR:
            printf("icr\n");
            break;

        case HV_X64_MSR_TPR:
            value = cpu_get_apic_tpr(X86_CPU(cpu)->apic_state);
            RAX(cpu) = (uint32_t)value;
            RDX(cpu) = (uint32_t)(value >> 32);
            break;
            
        case HV_X64_MSR_APIC_ASSIST_PAGE:
            abort();
            value = hyperv_cpu->msr_apic_assist;
            RAX(cpu) = (uint32_t)value;
            RDX(cpu) = (uint32_t)(value >> 32);
            break;

        default:
            //if ((msr >= HV_X64_MSR_GUEST_OS_ID) && (msr <= HV_MAX_MSR))
            //    printf("RDMSR to unimplemented Hyper-V MSR: 0x%lx\n", msr);
            value = 0;
            break;
    }

    if (partition_wide_msr)
        vmx_mutex_unlock(&hyperv_iface->partition_lock);

    return 0;
}

static void get_hypercall_parameters(
        struct CPUState* cpu,
        union hypercall_info* hypercall_info, 
        uint64_t* in_gpa, uint64_t* out_gpa)
{
    if (x86_is_long_mode(cpu)) {
        hypercall_info->qword = RCX(cpu);
        *in_gpa = RDX(cpu);
        *out_gpa = R8(cpu);
    } else {
        hypercall_info->qword = ((uint64_t)EDX(cpu) << 32) || EAX(cpu);
        *in_gpa = ((uint64_t)EBX(cpu) << 32) || ECX(cpu);
        *out_gpa = ((uint64_t)EDI(cpu) << 32) || ESI(cpu);
    }
}

static void set_hypercall_return_value(struct CPUState* cpu, uint64_t res, uint16_t rep_done)
{
    uint64_t ret = res || (((uint64_t)rep_done & 0xfff) << 32);

    if (x86_is_long_mode(cpu)) {
        RAX(cpu) = ret;
    } else {
        EDX(cpu) = ret >> 32;
        EAX(cpu) = ret & 0xffffffff;
    }
}

static void hyperv_hypercall_handler(struct CPUState* cpu)
{
    struct hyperv_interface* hyperv_iface = (struct hyperv_interface*)g_hypervisor_iface;
    union hypercall_info hypercall_info;
    uint64_t in_gpa, out_gpa;
    uint64_t res = HV_STATUS_INVALID_HYPERCALL_CODE;
    uint16_t rep_done = 0;

    /* Do nothing if hypercall is diabled (LSB of MSR is enable-bit) */
    if (!(hyperv_iface->msr_hypercall & 1))
        return;

    /* Hypercall generates #UD from non-zero CPL and real-mode (per Hyper-V spec) */
#if 0
    if (CPL(cpu) || (is_real(cpu))) {
        return;
    }
#endif

    get_hypercall_parameters(cpu, &hypercall_info, &in_gpa, &out_gpa);

    switch (hypercall_info.fields.call_code) {
    
        case HV_NOTIFY_LONG_SPIN_WAIT:
            res = HV_STATUS_SUCCESS;
            break;
    
        default:
            printf("Unhandled hypercall invoked by Guest OS: call_code=%d\n", hypercall_info.fields.call_code);
            break;
    }

    set_hypercall_return_value(cpu, res, rep_done);
}

#define HYPERV_VMSTATE_VERSION  (3)

static void hyperv_save_state(QEMUFile *f, void *opaque)
{
    struct hyperv_interface *hyperv_iface = (struct hyperv_interface*)opaque;
    uint8_t i;
    
    vmx_put_be32(f, HYPERV_VMSTATE_VERSION);

    vmx_put_be64(f, hyperv_iface->msr_hypercall);
    vmx_put_be64(f, hyperv_iface->msr_reference_tsc);
    vmx_put_be64(f, hyperv_iface->guest_os_version.qword);
    vmx_put_be64(f, vmx_clock_get_ns(QEMU_CLOCK_REALTIME) - hyperv_iface->rtclock_offset);
    for (i = 0 ; i < 5 ; ++i)
        vmx_put_be64(f, hyperv_iface->msr_crash_params[i]);
}

static int hyperv_load_state(QEMUFile *f, void *opaque, int version_id)
{
    struct hyperv_interface *hyperv_iface = (struct hyperv_interface*)opaque;
    uint8_t i = 0;

    uint32_t vmstate_version = vmx_get_be32(f);
    /* We currently don't have special handling for previous versions */
    if (vmstate_version != HYPERV_VMSTATE_VERSION)
        return (-1);    

    hyperv_iface->msr_hypercall = vmx_get_be64(f);
    hyperv_iface->msr_reference_tsc = vmx_get_be64(f);
    hyperv_iface->guest_os_version.qword = vmx_get_be64(f);
    hyperv_iface->rtclock_offset = vmx_clock_get_ns(QEMU_CLOCK_REALTIME) - vmx_get_be64(f);
    for (i = 0 ; i < 5 ; ++i)
        hyperv_iface->msr_crash_params[i] = vmx_get_be64(f);
    
    return 0;
}

extern int no_hyperv;

void init_hyperv_iface()
{
    if (no_hyperv)
        return;
    struct hyperv_interface* hyperv_iface = (struct hyperv_interface*)g_malloc0(sizeof(struct hyperv_interface));
    if (NULL == hyperv_iface) {
        printf("Failed to allocate memory for hyperv_interface\n");
        abort();
    }

    vmx_mutex_init(&hyperv_iface->partition_lock);

    g_hypervisor_iface = (struct known_hypervisor_interface*)hyperv_iface;
    g_hypervisor_iface->init_cpu_context = hyperv_init_cpu_context;
    g_hypervisor_iface->cpuid_handler = hyperv_cpuid_handler;
    g_hypervisor_iface->wrmsr_handler = hyperv_wrmsr_handler;
    g_hypervisor_iface->rdmsr_handler = hyperv_rdmsr_handler;
    g_hypervisor_iface->hypercall_handler = hyperv_hypercall_handler;

    hyperv_iface->rtclock_offset = vmx_clock_get_ns(QEMU_CLOCK_REALTIME);
    
    register_savevm(NULL, "hyperv", 0, 1, hyperv_save_state, hyperv_load_state, hyperv_iface);
}

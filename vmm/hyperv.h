
/* Microsoft Compatabile Hypervisor Interface support */

#ifndef __HYPERV_H__
#define __HYPERV_H__

#include "known_hypervisor_interface.h"
#include "qemu/thread-posix.h"

/* cpuid leafs */ 
#define HYPERV_ID                   (0x40000000)
#define HYPERV_VENDOR_NEUTRAL_ID    (0x40000001)
#define HYPERV_SYSTEM_ID            (0x40000002)
#define HYPERV_FEATURE_ID           (0x40000003)
#define HYPERV_ENLIGHTMENT_RECOMMENDATIONS     (0x40000004)
#define HYPERV_IMPLEMENTATION_LIMITS           (0x40000005)
#define HYPERV_IMPLEMENTATION_HW_FEATURES      (0x400fffff)
#define HYPERV_NEXT_HYPERVISOR_ID              (0x40000100)

/* cpuid.HYPERV_FEATURE_ID consts */
#define CPUID_AccessVpRunTimeMsr                (1 << 0)
#define CPUID_AccessPartitionReferenceCounter   (1 << 1)
#define CPUID_AccessSynICMsrs                   (1 << 2)
#define CPUID_AccessSyntheticTimerMsrs          (1 << 3)
#define CPUID_AccessApicMsrs                    (1 << 4)
#define CPUID_AccessHypercallMsrs               (1 << 5)
#define CPUID_AccessVpIndex                     (1 << 6)
#define CPUID_AccessResetMsr                    (1 << 7)
#define CPUID_AccessStatsMsr                    (1 << 8)
#define CPUID_AccessPartitionReferenceTsc       (1 << 9)
#define CPUID_AccessGuestIdleMsr                (1 << 10)
#define CPUID_AccessFrequencyMsrs               (1 << 11)
#define CPUID_AccessDebugMsrs                   (1 << 12)
/* Bits 13-31 are reserved */

/* cpuid.HYPERV_ENLIGHTMENT_RECOMMENDATIONS consts */
#define CPUID_HypercallMovCr3           (1 << 0)
#define CPUID_HypercallLocalTLBFlush    (1 << 1)
#define CPUID_HypercallRemoteTLBFlush   (1 << 2)
#define CPUID_UseMSRsForAPICAccess      (1 << 3)
#define CPUID_MSRSystemReset            (1 << 4)
#define CPUID_UseRelaxedTimingInCurrentParrtition     (1 << 5)
#define CPUID_UseDMARemapping           (1 << 6)
#define CPUID_UseInterruptRemapping     (1 << 7)
#define CPUID_Usex2APICMsrs             (1 << 8)
#define CPUID_DeprecateAutoEOI          (1 << 9)
/* Bits 6-31 are reserved */

/* cpuid.HYPERV_IMPLEMENTATION_HW_FEATURES consts */
#define CPUID_ApicOverlayAssist             (1 << 0)
#define CPUID_MSRBitmap                     (1 << 1)
#define CPUID_PerformanceCounters           (1 << 2)
#define CPUID_SecondLevelAddressTranslation (1 << 3)
#define CPUID_DMARemapping                  (1 << 4)
#define CPUID_InterruptRemapping            (1 << 5)
#define CPUID_MemoryPatrolScrubberPresent   (1 << 6)
/* Bits 7-31 are reserved */

/* Hypervisor Synthetic MSRs */
#define HV_X64_MSR_GUEST_OS_ID          0x40000000  /* required */
#define HV_X64_MSR_HYPERCALL            0x40000001  /* required */
#define HV_X64_MSR_VP_INDEX             0x40000002  /* required */
#define HV_X64_MSR_TIME_REF_COUNT       0x40000020
#define HV_X64_MSR_TIME_REFERENCE_TSC   0x40000021

/* Hypervisor Synthetic MSRs related to Synthetic APIC */
#define HV_X64_MSR_EOI                  0x40000070
#define HV_X64_MSR_ICR                  0x40000071
#define HV_X64_MSR_TPR                  0x40000072
#define HV_X64_MSR_APIC_ASSIST_PAGE     0x40000073

/* Hypervisor Synthetic MSRs related to Guest OS Crash */
#define HV_X64_MSR_CRASH_P0             0x40000100
#define HV_X64_MSR_CRASH_P1             0x40000101
#define HV_X64_MSR_CRASH_P2             0x40000102
#define HV_X64_MSR_CRASH_P3             0x40000103
#define HV_X64_MSR_CRASH_P4             0x40000104
#define HV_X64_MSR_CRASH_CTL            0x40000105

/* Max MSR index which is related to Microsoft Compatible Hypervisor Interface */
#define HV_MAX_MSR                      0x4000009F

/* HV_MSR_CRASH_CTL bits */
#define HV_MSR_CRASH_CTL_CRASH_NOTIFY       (1ULL << 63)

/* Hypercall status codes */
#define HV_STATUS_SUCCESS                   (0x0000)
#define HV_STATUS_INVALID_HYPERCALL_CODE    (0x0002)

/* Hypercall call codes */
#define HV_NOTIFY_LONG_SPIN_WAIT            (0x0008)

union guest_os_version {
    struct {
        uint16_t build_number;
        uint8_t service_version;
        uint8_t minor_version;
        uint8_t major_version;
        uint8_t os_id;
        uint16_t vendor_id:15;
        uint8_t os_type:1;
    } fields;
    uint64_t qword;
} __attribute__((packed));

struct hyperv_interface {
    struct known_hypervisor_interface hv_iface;

    QemuMutex partition_lock;
    uint64_t msr_hypercall;
    uint64_t msr_reference_tsc;
    union guest_os_version guest_os_version;
    uint64_t rtclock_offset;
    uint64_t msr_crash_params[5];
};

struct hyperv_cpu_context {
    uint64_t msr_apic_assist;
};

struct hv_reference_tsc_page {
    volatile uint32_t tsc_sequence;
    uint32_t reserved1;
    volatile uint64_t tsc_scale;
    volatile int64_t tsc_offset;
};

union hypercall_info {
    struct {
        uint64_t call_code:16;
        uint64_t fast:1;
        uint64_t rsrvd1:15;
        uint64_t rep_count:12;
        uint64_t rsrvd2:4;
        uint64_t rep_start_index:12;
        uint64_t rsrvd3:4;
    } fields;
    uint64_t qword;
};

void init_hyperv_iface();

#endif /* __HYPERV_H__ */


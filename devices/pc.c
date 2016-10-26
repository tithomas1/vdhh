/*
 * QEMU PC System Emulator
 *
 * Copyright (c) 2003-2004 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "hw.h"
#include "ipc.h"
#include "iapic.h"
#include "ide.h"
#include "pci.h"
#include "monitor/monitor.h"
#include "fw_cfg.h"
#include "hpet.h"
#include "ismbios.h"
#include "loader.h"
#include "multiboot.h"
#include "mc146818rtc.h"
#include "i8254.h"
#include "pcspk.h"
#include "qsysbus.h"
#include "sysemu.h"
#include "veertuemu.h"
#include "vmm/veertu_vmx.h"
#include "emublock-backend.h"

#include "memory.h"
#include "address-spaces.h"
#include "emuarch_init.h"
#include "qemu/bitmap.h"
#include "qemu/config-file.h"
#include "nacpi.h"
#include "icc_bus.h"
#include "boards.h"
#include "pci_host.h"
#include "acpi-build.h"
#include "qapi/visitor.h"
#include "qapi-visit.h"
#include "vmm/vmx.h"
#include "iapic_internal.h"

/* debug PC/ISA interrupts */
//#define DEBUG_IRQ

#ifdef DEBUG_IRQ
#define DPRINTF(fmt, ...)                                       \
    do { printf("CPUIRQ: " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...)
#endif

/* Leave a chunk of memory at the top of RAM for the BIOS ACPI tables
 * (128K) and other BIOS datastructures (less than 4K reported to be used at
 * the moment, 32K should be enough for a while).  */
static unsigned acpi_data_size = 0x20000 + 0x8000;
void pc_set_legacy_acpi_data_size(void)
{
    acpi_data_size = 0x10000;
}

#define BIOS_CFG_IOPORT 0x510
#define FW_CFG_ACPI_TABLES (FW_CFG_ARCH_LOCAL + 0)
#define FW_CFG_SMBIOS_ENTRIES (FW_CFG_ARCH_LOCAL + 1)
#define FW_CFG_IRQ0_OVERRIDE (FW_CFG_ARCH_LOCAL + 2)
#define FW_CFG_E820_TABLE (FW_CFG_ARCH_LOCAL + 3)
#define FW_CFG_HPET (FW_CFG_ARCH_LOCAL + 4)

#define E820_NR_ENTRIES		16

struct e820_entry {
    uint64_t address;
    uint64_t length;
    uint32_t type;
} QEMU_PACKED __attribute((__aligned__(4)));

struct e820_table {
    uint32_t count;
    struct e820_entry entry[E820_NR_ENTRIES];
} QEMU_PACKED __attribute((__aligned__(4)));

static struct e820_table e820_reserve;
static struct e820_entry *e820_table;
static unsigned e820_entries;
struct hpet_fw_config hpet_cfg = {.count = UINT8_MAX};

extern struct DeviceState *pic_dev1;
extern struct DeviceState *pic_dev2;
extern struct DeviceState *apic_dev;

extern void pic_set_irq(void *opaque, int irq, int level);
extern void ioapic_set_irq(void *opaque, int irq, int level);

void gsi_handler(void *opaque, int n, int level)
{
    GSIState *s = opaque;

    DPRINTF("pc: %s GSI %d\n", level ? "raising" : "lowering", n);
    if (n < ISA_NUM_IRQS) {
        if (n >= 0) {
            if (n < 8)
                pic_set_irq(pic_dev1, n, level);
            else
                pic_set_irq(pic_dev2, n, level);
        }
    }
    
    if (n >= 0)
        ioapic_set_irq(apic_dev, n, level);
}

static void ioport80_write(void *opaque, hwaddr addr, uint64_t data,
                           unsigned size)
{
}

static uint64_t ioport80_read(void *opaque, hwaddr addr, unsigned size)
{
    return 0xffffffffffffffffULL;
}

/* MSDOS compatibility mode FPU exception support */
static vmx_irq ferr_irq;

void pc_register_ferr_irq(vmx_irq irq)
{
    ferr_irq = irq;
}

/* XXX: add IGNNE support */
void cpu_set_ferr(CPUX86State *s)
{
    vmx_irq_raise(ferr_irq);
}

static void ioportF0_write(void *opaque, hwaddr addr, uint64_t data,
                           unsigned size)
{
    vmx_irq_lower(ferr_irq);
}

static uint64_t ioportF0_read(void *opaque, hwaddr addr, unsigned size)
{
    return 0xffffffffffffffffULL;
}

/* TSC handling */
uint64_t cpu_get_tsc(CPUX86State *env)
{
    return cpu_get_ticks();
}

/* SMM support */

static cpu_set_smm_t smm_set;
static void *smm_arg;

void cpu_smm_register(cpu_set_smm_t callback, void *arg)
{
    assert(smm_set == NULL);
    assert(smm_arg == NULL);
    smm_set = callback;
    smm_arg = arg;
}

void cpu_smm_update(CPUX86State *env)
{
    if (smm_set && smm_arg && GETCPU(x86_env_get_cpu(env)) == first_cpu) {
        smm_set(!!(env->hflags & HF_SMM_MASK), smm_arg);
    }
}


/* IRQ handling */
int cpu_get_pic_interrupt(CPUX86State *env)
{
    X86CPU *cpu = x86_env_get_cpu(env);
    int intno;

    intno = apic_get_interrupt(cpu->apic_state);
    if (intno >= 0) {
        return intno;
    }
    /* read the irq from the PIC */
    if (!apic_accept_pic_intr(cpu->apic_state)) {
        return -1;
    }

    intno = pic_read_irq(isa_pic);
    return intno;
}

static void pic_irq_request(void *opaque, int irq, int level)
{
    CPUState *cs = first_cpu;
    X86CPU *cpu = X86_CPU(cs);

    DPRINTF("pic_irqs: %s irq %d\n", level? "raise" : "lower", irq);
    if (cpu->apic_state) {
        CPU_FOREACH(cs) {
            cpu = X86_CPU(cs);
            if (apic_accept_pic_intr(cpu->apic_state)) {
                apic_deliver_pic_intr(cpu->apic_state, level);
            }
        }
    } else {
        if (level) {
            cpu_interrupt(cs, CPU_INTERRUPT_HARD);
        } else {
            cpu_reset_interrupt(cs, CPU_INTERRUPT_HARD);
        }
    }
}

/* PC cmos mappings */

#define REG_EQUIPMENT_BYTE          0x14


static void cmos_init_hd(ISADevice *s, int type_ofs, int info_ofs,
                         int16_t cylinders, int8_t heads, int8_t sectors)
{
    rtc_set_memory(s, type_ofs, 47);
    rtc_set_memory(s, info_ofs, cylinders);
    rtc_set_memory(s, info_ofs + 1, cylinders >> 8);
    rtc_set_memory(s, info_ofs + 2, heads);
    rtc_set_memory(s, info_ofs + 3, 0xff);
    rtc_set_memory(s, info_ofs + 4, 0xff);
    rtc_set_memory(s, info_ofs + 5, 0xc0 | ((heads > 8) << 3));
    rtc_set_memory(s, info_ofs + 6, cylinders);
    rtc_set_memory(s, info_ofs + 7, cylinders >> 8);
    rtc_set_memory(s, info_ofs + 8, sectors);
}

/* convert boot_device letter to something recognizable by the bios */
static int boot_device2nibble(char boot_device)
{
    switch(boot_device) {
    case 'a':
    case 'b':
        return 0x01; /* floppy boot */
    case 'c':
        return 0x02; /* hard drive boot */
    case 'd':
        return 0x03; /* CD-ROM boot */
    case 'n':
        return 0x04; /* Network boot */
    }
    return 0;
}

static void set_boot_dev(ISADevice *s, const char *boot_device, Error **errp)
{
#define PC_MAX_BOOT_DEVICES 3
    int nbds, bds[3] = { 0, };
    int i;

    nbds = strlen(boot_device);
    if (nbds > PC_MAX_BOOT_DEVICES) {
        error_setg(errp, "Too many boot devices for PC");
        return;
    }
    for (i = 0; i < nbds; i++) {
        bds[i] = boot_device2nibble(boot_device[i]);
        if (bds[i] == 0) {
            error_setg(errp, "Invalid boot device for PC: '%c'",
                       boot_device[i]);
            return;
        }
    }
    rtc_set_memory(s, 0x3d, (bds[1] << 4) | bds[0]);
    rtc_set_memory(s, 0x38, (bds[2] << 4) | (fd_bootchk ? 0x0 : 0x1));
}

static void pc_boot_set(void *opaque, const char *boot_device, Error **errp)
{
    set_boot_dev(opaque, boot_device, errp);
}

typedef struct pc_cmos_init_late_arg {
    ISADevice *rtc_state;
    BusState *idebus[2];
} pc_cmos_init_late_arg;

static void pc_cmos_init_late(void *opaque)
{
    pc_cmos_init_late_arg *arg = opaque;
    ISADevice *s = arg->rtc_state;
    int16_t cylinders;
    int8_t heads, sectors;
    int val;
    int i, trans;

    val = 0;
    if (ide_get_geometry(arg->idebus[0], 0,
                         &cylinders, &heads, &sectors) >= 0) {
        cmos_init_hd(s, 0x19, 0x1b, cylinders, heads, sectors);
        val |= 0xf0;
    }
    if (ide_get_geometry(arg->idebus[0], 1,
                         &cylinders, &heads, &sectors) >= 0) {
        cmos_init_hd(s, 0x1a, 0x24, cylinders, heads, sectors);
        val |= 0x0f;
    }
    rtc_set_memory(s, 0x12, val);

    val = 0;
    for (i = 0; i < 4; i++) {
        /* NOTE: ide_get_geometry() returns the physical
           geometry.  It is always such that: 1 <= sects <= 63, 1
           <= heads <= 16, 1 <= cylinders <= 16383. The BIOS
           geometry can be different if a translation is done. */
        if (ide_get_geometry(arg->idebus[i / 2], i % 2,
                             &cylinders, &heads, &sectors) >= 0) {
            trans = ide_get_bios_chs_trans(arg->idebus[i / 2], i % 2) - 1;
            assert((trans & ~3) == 0);
            val |= trans << (i * 2);
        }
    }
    rtc_set_memory(s, 0x39, val);

    vmx_unregister_reset(pc_cmos_init_late, opaque);
}

void pc_cmos_init(ram_addr_t ram_size, ram_addr_t above_4g_mem_size,
                  const char *boot_device, MachineState *machine,
                  ISADevice *floppy, BusState *idebus0, BusState *idebus1,
                  ISADevice *s)
{
    int val, nb, i;
    static pc_cmos_init_late_arg arg;
    PCMachineState *pc_machine = PC_MACHINE(machine);
    Error *local_err = NULL;

    /* various important CMOS locations needed by PC/Bochs bios */

    /* memory size */
    /* base memory (first MiB) */
    val = MIN(ram_size / 1024, 640);
    rtc_set_memory(s, 0x15, val);
    rtc_set_memory(s, 0x16, val >> 8);
    /* extended memory (next 64MiB) */
    if (ram_size > 1024 * 1024) {
        val = (ram_size - 1024 * 1024) / 1024;
    } else {
        val = 0;
    }
    if (val > 65535)
        val = 65535;
    rtc_set_memory(s, 0x17, val);
    rtc_set_memory(s, 0x18, val >> 8);
    rtc_set_memory(s, 0x30, val);
    rtc_set_memory(s, 0x31, val >> 8);
    /* memory between 16MiB and 4GiB */
    if (ram_size > 16 * 1024 * 1024) {
        val = (ram_size - 16 * 1024 * 1024) / 65536;
    } else {
        val = 0;
    }
    if (val > 65535)
        val = 65535;
    rtc_set_memory(s, 0x34, val);
    rtc_set_memory(s, 0x35, val >> 8);
    /* memory above 4GiB */
    val = above_4g_mem_size / 65536;
    rtc_set_memory(s, 0x5b, val);
    rtc_set_memory(s, 0x5c, val >> 8);
    rtc_set_memory(s, 0x5d, val >> 16);

    /* set the number of GETCPU */
    rtc_set_memory(s, 0x5f, smp_cpus - 1);


    set_boot_dev(s, boot_device, &local_err);
    if (local_err) {
        error_report("%s", error_get_pretty(local_err));
        exit(1);
    }


    rtc_set_memory(s, 0x10, val);

    val = 0;
    nb = 0;

    switch (nb) {
    case 0:
        break;
    case 1:
        val |= 0x01; /* 1 drive, ready for boot */
        break;
    case 2:
        val |= 0x41; /* 2 drives, ready for boot */
        break;
    }
    val |= 0x02; /* FPU is there */
    val |= 0x04; /* PS/2 mouse installed */
    rtc_set_memory(s, REG_EQUIPMENT_BYTE, val);

    /* hard drives */
    arg.rtc_state = s;
    arg.idebus[0] = idebus0;
    arg.idebus[1] = idebus1;
    vmx_register_reset(pc_cmos_init_late, &arg);
}

#define TYPE_PORT92 "port92"
#define PORT92(obj) obj

/* port 92 stuff: could be split off */
typedef struct Port92State {
    ISADevice parent;

    VeertuMemArea io;
    uint8_t outport;
    vmx_irq *a20_out;
} Port92State;

static void port92_write(void *opaque, hwaddr addr, uint64_t val,
                         unsigned size)
{
    Port92State *s = opaque;
    int oldval = s->outport;

    DPRINTF("port92: write 0x%02" PRIx64 "\n", val);
    s->outport = val;
    vmx_set_irq(*s->a20_out, (val >> 1) & 1);
    if ((val & 1) && !(oldval & 1)) {
        vmx_system_reset_request();
    }
}

static uint64_t port92_read(void *opaque, hwaddr addr,
                            unsigned size)
{
    Port92State *s = opaque;
    uint32_t ret;

    ret = s->outport;
    DPRINTF("port92: read 0x%02x\n", ret);
    return ret;
}

static void port92_init(ISADevice *dev, vmx_irq *a20_out)
{
    Port92State *s = PORT92(dev);

    s->a20_out = a20_out;
}

static const VMStateDescription vmstate_port92_isa = {
    .name = "port92",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT8(outport, Port92State),
        VMSTATE_END_OF_LIST()
    }
};

static void port92_reset(DeviceState *d)
{
    Port92State *s = PORT92(d);

    s->outport &= ~1;
}

static const MemAreaOps port92_ops = {
    .read = port92_read,
    .write = port92_write,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void port92_initfn(VeertuType *obj)
{
    Port92State *s = PORT92(obj);

    memory_area_init_io(&s->io, VeertuTypeHold(s), &port92_ops, s, "port92", 1);

    s->outport = 0;
}

static void port92_realizefn(DeviceState *dev, Error **errp)
{
    ISADevice *isadev = ISA_DEVICE(dev);
    Port92State *s = PORT92(dev);

    isa_register_ioport(isadev, &s->io, 0x92);
}

static void port92_class_initfn(VeertuTypeClassHold *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = port92_realizefn;
    dc->reset = port92_reset;
    dc->vmsd = &vmstate_port92_isa;
    /*
     * Reason: unlike ordinary ISA devices, this one needs additional
     * wiring: its A20 output line needs to be wired up by
     * port92_init().
     */
    dc->cannot_instantiate_with_device_add_yet = true;
}

static const VeertuTypeInfo port92_info = {
    .name          = TYPE_PORT92,
    .parent        = TYPE_ISA_DEVICE,
    .instance_size = sizeof(Port92State),
    .instance_init = port92_initfn,
    .class_init    = port92_class_initfn,
};

void port92_register_types(void)
{
    register_type_internal(&port92_info);
}

static void handle_a20_line_change(void *opaque, int irq, int level)
{
    X86CPU *cpu = opaque;

    /* XXX: send to all CPUs ? */
    /* XXX: add logic to handle multiple A20 line sources */
    x86_cpu_set_a20(cpu, level);
}

int e820_add_entry(uint64_t address, uint64_t length, uint32_t type)
{
    int index = le32_to_cpu(e820_reserve.count);
    struct e820_entry *entry;

    if (type != E820_RAM) {
        /* old FW_CFG_E820_TABLE entry -- reservations only */
        if (index >= E820_NR_ENTRIES) {
            return -EBUSY;
        }
        entry = &e820_reserve.entry[index++];

        entry->address = cpu_to_le64(address);
        entry->length = cpu_to_le64(length);
        entry->type = cpu_to_le32(type);

        e820_reserve.count = cpu_to_le32(index);
    }

    /* new "etc/e820" file -- include ram too */
    e820_table = g_renew(struct e820_entry, e820_table, e820_entries + 1);
    e820_table[e820_entries].address = cpu_to_le64(address);
    e820_table[e820_entries].length = cpu_to_le64(length);
    e820_table[e820_entries].type = cpu_to_le32(type);
    e820_entries++;

    return e820_entries;
}

int e820_get_num_entries(void)
{
    return e820_entries;
}

bool e820_get_entry(int idx, uint32_t type, uint64_t *address, uint64_t *length)
{
    if (idx < e820_entries && e820_table[idx].type == cpu_to_le32(type)) {
        *address = le64_to_cpu(e820_table[idx].address);
        *length = le64_to_cpu(e820_table[idx].length);
        return true;
    }
    return false;
}

/* Calculates the limit to GETCPU APIC ID values
 *
 * This function returns the limit for the APIC ID value, so that all
 * GETCPU APIC IDs are < pc_apic_id_limit().
 *
 * This is used for FW_CFG_MAX_CPUS. See comments on bochs_bios_init().
 */
static unsigned int pc_apic_id_limit(unsigned int max_cpus)
{
    return x86_cpu_apic_id_from_index(max_cpus - 1) + 1;
}

static FWCfgState *bochs_bios_init(void)
{
    FWCfgState *fw_cfg;
    uint8_t *smbios_tables, *smbios_anchor;
    size_t smbios_tables_len, smbios_anchor_len;
    uint64_t *numa_fw_cfg;
    int i, j;
    unsigned int apic_id_limit = pc_apic_id_limit(max_cpus);

    fw_cfg = fw_cfg_init_io(BIOS_CFG_IOPORT);
    /* FW_CFG_MAX_CPUS is a bit confusing/problematic on x86:
     *
     * SeaBIOS needs FW_CFG_MAX_CPUS for GETCPU hotplug, but the GETCPU hotplug
     * QEMU<->SeaBIOS interface is not based on the "GETCPU index", but on the APIC
     * ID of hotplugged CPUs[1]. This means that FW_CFG_MAX_CPUS is not the
     * "maximum number of CPUs", but the "limit to the APIC ID values SeaBIOS
     * may see".
     *
     * So, this means we must not use max_cpus, here, but the maximum possible
     * APIC ID value, plus one.
     *
     * [1] The only kind of "GETCPU identifier" used between SeaBIOS and QEMU is
     *     the APIC ID, not the "GETCPU index"
     */
    fw_cfg_add_i16(fw_cfg, FW_CFG_MAX_CPUS, (uint16_t)apic_id_limit);
    fw_cfg_add_i32(fw_cfg, FW_CFG_ID, 1);
    fw_cfg_add_i64(fw_cfg, FW_CFG_RAM_SIZE, (uint64_t)ram_size);
    fw_cfg_add_bytes(fw_cfg, FW_CFG_ACPI_TABLES,
                     acpi_tables, acpi_tables_len);
    fw_cfg_add_i32(fw_cfg, FW_CFG_IRQ0_OVERRIDE, veertu_allow_irq0_override());

    smbios_tables = smbios_get_table_legacy(&smbios_tables_len);
    if (smbios_tables) {
        fw_cfg_add_bytes(fw_cfg, FW_CFG_SMBIOS_ENTRIES,
                         smbios_tables, smbios_tables_len);
    }

    if (smbios_anchor) {
        fw_cfg_add_file(fw_cfg, "etc/smbios/smbios-tables",
                        smbios_tables, smbios_tables_len);
        fw_cfg_add_file(fw_cfg, "etc/smbios/smbios-anchor",
                        smbios_anchor, smbios_anchor_len);
    }

    fw_cfg_add_bytes(fw_cfg, FW_CFG_E820_TABLE,
                     &e820_reserve, sizeof(e820_reserve));
    fw_cfg_add_file(fw_cfg, "etc/e820", e820_table,
                    sizeof(struct e820_entry) * e820_entries);

    fw_cfg_add_bytes(fw_cfg, FW_CFG_HPET, &hpet_cfg, sizeof(hpet_cfg));
    return fw_cfg;
}

static long get_file_size(FILE *f)
{
    long where, size;

    /* XXX: on Unix systems, using fstat() probably makes more sense */

    where = ftell(f);
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, where, SEEK_SET);

    return size;
}

static void load_linux(FWCfgState *fw_cfg,
                       const char *kernel_filename,
                       const char *initrd_filename,
                       const char *kernel_cmdline,
                       hwaddr max_ram_size)
{
    uint16_t protocol;
    int setup_size, kernel_size, initrd_size = 0, cmdline_size;
    uint32_t initrd_max;
    uint8_t header[8192], *setup, *kernel, *initrd_data;
    hwaddr real_addr, prot_addr, cmdline_addr, initrd_addr = 0;
    FILE *f;
    char *vmode;

    /* Align to 16 bytes as a paranoia measure */
    cmdline_size = (strlen(kernel_cmdline)+16) & ~15;

    /* load the kernel header */
    f = fopen(kernel_filename, "rb");
    if (!f || !(kernel_size = get_file_size(f)) ||
        fread(header, 1, MIN(ARRAY_SIZE(header), kernel_size), f) !=
        MIN(ARRAY_SIZE(header), kernel_size)) {
        fprintf(stderr, "veertu: could not load kernel '%s': %s\n",
                kernel_filename, strerror(errno));
        exit(1);
    }

    /* kernel protocol version */
#if 0
    fprintf(stderr, "header magic: %#x\n", ldl_p(header+0x202));
#endif
    if (ldl_p(header+0x202) == 0x53726448) {
        protocol = lduw_p(header+0x206);
    } else {
        /* This looks like a multiboot kernel. If it is, let's stop
           treating it like a Linux kernel. */
        if (load_multiboot(fw_cfg, f, kernel_filename, initrd_filename,
                           kernel_cmdline, kernel_size, header)) {
            return;
        }
        protocol = 0;
    }

    if (protocol < 0x200 || !(header[0x211] & 0x01)) {
        /* Low kernel */
        real_addr    = 0x90000;
        cmdline_addr = 0x9a000 - cmdline_size;
        prot_addr    = 0x10000;
    } else if (protocol < 0x202) {
        /* High but ancient kernel */
        real_addr    = 0x90000;
        cmdline_addr = 0x9a000 - cmdline_size;
        prot_addr    = 0x100000;
    } else {
        /* High and recent kernel */
        real_addr    = 0x10000;
        cmdline_addr = 0x20000;
        prot_addr    = 0x100000;
    }

#if 0
    fprintf(stderr,
            "veertu: real_addr     = 0x" TARGET_FMT_plx "\n"
            "veertu: cmdline_addr  = 0x" TARGET_FMT_plx "\n"
            "veertu: prot_addr     = 0x" TARGET_FMT_plx "\n",
            real_addr,
            cmdline_addr,
            prot_addr);
#endif

    /* highest address for loading the initrd */
    if (protocol >= 0x203) {
        initrd_max = ldl_p(header+0x22c);
    } else {
        initrd_max = 0x37ffffff;
    }

    if (initrd_max >= max_ram_size - acpi_data_size) {
        initrd_max = max_ram_size - acpi_data_size - 1;
    }

    fw_cfg_add_i32(fw_cfg, FW_CFG_CMDLINE_ADDR, cmdline_addr);
    fw_cfg_add_i32(fw_cfg, FW_CFG_CMDLINE_SIZE, strlen(kernel_cmdline)+1);
    fw_cfg_add_string(fw_cfg, FW_CFG_CMDLINE_DATA, kernel_cmdline);

    if (protocol >= 0x202) {
        stl_p(header+0x228, cmdline_addr);
    } else {
        stw_p(header+0x20, 0xA33F);
        stw_p(header+0x22, cmdline_addr-real_addr);
    }

    /* handle vga= parameter */
    vmode = strstr(kernel_cmdline, "vga=");
    if (vmode) {
        unsigned int video_mode;
        /* skip "vga=" */
        vmode += 4;
        if (!strncmp(vmode, "normal", 6)) {
            video_mode = 0xffff;
        } else if (!strncmp(vmode, "ext", 3)) {
            video_mode = 0xfffe;
        } else if (!strncmp(vmode, "ask", 3)) {
            video_mode = 0xfffd;
        } else {
            video_mode = strtol(vmode, NULL, 0);
        }
        stw_p(header+0x1fa, video_mode);
    }

    /* loader type */
    /* High nybble = B reserved for QEMU; low nybble is revision number.
       If this code is substantially changed, you may want to consider
       incrementing the revision. */
    if (protocol >= 0x200) {
        header[0x210] = 0xB0;
    }
    /* heap */
    if (protocol >= 0x201) {
        header[0x211] |= 0x80;	/* CAN_USE_HEAP */
        stw_p(header+0x224, cmdline_addr-real_addr-0x200);
    }

    /* load initrd */
    if (initrd_filename) {
        if (protocol < 0x200) {
            fprintf(stderr, "veertu: linux kernel too old to load a ram disk\n");
            exit(1);
        }

        initrd_size = get_image_size(initrd_filename);
        if (initrd_size < 0) {
            fprintf(stderr, "veertu: error reading initrd %s: %s\n",
                    initrd_filename, strerror(errno));
            exit(1);
        }

        initrd_addr = (initrd_max-initrd_size) & ~4095;

        initrd_data = g_malloc(initrd_size);
        load_image(initrd_filename, initrd_data);

        fw_cfg_add_i32(fw_cfg, FW_CFG_INITRD_ADDR, initrd_addr);
        fw_cfg_add_i32(fw_cfg, FW_CFG_INITRD_SIZE, initrd_size);
        fw_cfg_add_bytes(fw_cfg, FW_CFG_INITRD_DATA, initrd_data, initrd_size);

        stl_p(header+0x218, initrd_addr);
        stl_p(header+0x21c, initrd_size);
    }

    /* load kernel and setup */
    setup_size = header[0x1f1];
    if (setup_size == 0) {
        setup_size = 4;
    }
    setup_size = (setup_size+1)*512;
    kernel_size -= setup_size;

    setup  = g_malloc(setup_size);
    kernel = g_malloc(kernel_size);
    fseek(f, 0, SEEK_SET);
    if (fread(setup, 1, setup_size, f) != setup_size) {
        fprintf(stderr, "fread() failed\n");
        exit(1);
    }
    if (fread(kernel, 1, kernel_size, f) != kernel_size) {
        fprintf(stderr, "fread() failed\n");
        exit(1);
    }
    fclose(f);
    memcpy(setup, header, MIN(sizeof(header), setup_size));

    fw_cfg_add_i32(fw_cfg, FW_CFG_KERNEL_ADDR, prot_addr);
    fw_cfg_add_i32(fw_cfg, FW_CFG_KERNEL_SIZE, kernel_size);
    fw_cfg_add_bytes(fw_cfg, FW_CFG_KERNEL_DATA, kernel, kernel_size);

    fw_cfg_add_i32(fw_cfg, FW_CFG_SETUP_ADDR, real_addr);
    fw_cfg_add_i32(fw_cfg, FW_CFG_SETUP_SIZE, setup_size);
    fw_cfg_add_bytes(fw_cfg, FW_CFG_SETUP_DATA, setup, setup_size);

    option_rom[nb_option_roms].name = "linuxboot.bin";
    option_rom[nb_option_roms].bootindex = 0;
    nb_option_roms++;
}

#define NE2000_NB_MAX 6

static const int ne2000_io[NE2000_NB_MAX] = { 0x300, 0x320, 0x340, 0x360,
                                              0x280, 0x380 };
static const int ne2000_irq[NE2000_NB_MAX] = { 9, 10, 11, 3, 4, 5 };

void pc_init_ne2k_isa(ISABus *bus, NICInfo *nd)
{
    static int nb_ne2k = 0;

    if (nb_ne2k == NE2000_NB_MAX)
        return;
    isa_ne2000_init(bus, ne2000_io[nb_ne2k],
                    ne2000_irq[nb_ne2k], nd);
    nb_ne2k++;
}

DeviceState *cpu_get_current_apic(void)
{
    if (current_cpu) {
        X86CPU *cpu = X86_CPU(current_cpu);
        return cpu->apic_state;
    } else {
        return NULL;
    }
}

void pc_acpi_smi_interrupt(void *opaque, int irq, int level)
{
    X86CPU *cpu = opaque;

    if (level) {
        cpu_interrupt(GETCPU(cpu), CPU_INTERRUPT_SMI);
    }
}

static X86CPU *pc_new_cpu(const char *cpu_model, int64_t apic_id,
                          DeviceState *icc_bridge, Error **errp)
{
    X86CPU *cpu;
    Error *local_err = NULL;
    APICCommonState *s;

    cpu = cpu_x86_create(cpu_model, icc_bridge, &local_err);
    if (local_err != NULL) {
        error_propagate(errp, local_err);
        return NULL;
    }

    device_set_realized(VeertuTypeHold(cpu), true, &local_err);

    if (local_err) {
        error_propagate(errp, local_err);
        cpu = NULL;
    }
    return cpu;
}

static const char *current_cpu_model;

void pc_cpus_init(const char *cpu_model, DeviceState *icc_bridge)
{
    int i;
    X86CPU *cpu = NULL;
    Error *error = NULL;
    unsigned long apic_id_limit;

    /* init CPUs */
    if (cpu_model == NULL) {
#ifdef TARGET_X86_64
        cpu_model = "vmx64";
#else
        cpu_model = "vmx32";
#endif
    }
    current_cpu_model = cpu_model;

    apic_id_limit = pc_apic_id_limit(max_cpus);
    if (apic_id_limit > 256) {
        error_report("max_cpus is too large. APIC ID of last GETCPU is %lu",
                     apic_id_limit - 1);
        exit(1);
    }

    for (i = 0; i < smp_cpus; i++) {
        cpu = pc_new_cpu(cpu_model, x86_cpu_apic_id_from_index(i),
                         icc_bridge, &error);
        if (error) {
            error_report("%s", error_get_pretty(error));
            error_free(error);
            exit(1);
        }
    }

    /* map APIC MMIO area if GETCPU has APIC */
    if (cpu && cpu->apic_state) {
        /* XXX: what if the base changes? */
        sysbus_mmio_map_overlap(SYS_BUS_DEVICE(icc_bridge), 0,
                                APIC_DEFAULT_ADDRESS, 0x1000);
    }

    /* tell smbios about cpuid version and features */
    //smbios_set_cpuid(cpu->env.cpuid_version, cpu->env.features[FEAT_1_EDX]);
}

/* pci-info ROM file. Little endian format */
typedef struct PcRomPciInfo {
    uint64_t w32_min;
    uint64_t w32_max;
    uint64_t w64_min;
    uint64_t w64_max;
} PcRomPciInfo;

typedef struct PcGuestInfoState {
    PcGuestInfo info;
    Notifier machine_done;
} PcGuestInfoState;

static
void pc_guest_info_machine_done(Notifier *notifier, void *data)
{
    PcGuestInfoState *guest_info_state = container_of(notifier,
                                                      PcGuestInfoState,
                                                      machine_done);
    acpi_setup(&guest_info_state->info);
}

PcGuestInfo *pc_guest_info_init(ram_addr_t below_4g_mem_size,
                                ram_addr_t above_4g_mem_size)
{
    PcGuestInfoState *guest_info_state = g_malloc0(sizeof *guest_info_state);
    PcGuestInfo *guest_info = &guest_info_state->info;
    int i, j;

    guest_info->ram_size_below_4g = below_4g_mem_size;
    guest_info->ram_size = below_4g_mem_size + above_4g_mem_size;
    guest_info->apic_id_limit = pc_apic_id_limit(max_cpus);
    guest_info->apic_xrupt_override = veertu_allow_irq0_override();
    guest_info->numa_nodes = nb_numa_nodes;
    guest_info->node_mem = g_malloc0(guest_info->numa_nodes *
                                    sizeof *guest_info->node_mem);

    guest_info->node_cpu = g_malloc0(guest_info->apic_id_limit *
                                     sizeof *guest_info->node_cpu);

    guest_info_state->machine_done.notify = pc_guest_info_machine_done;
    vmx_add_machine_init_done_notifier(&guest_info_state->machine_done);
    return guest_info;
}

/* setup pci memory address space mapping into system address space */
void pc_pci_as_mapping_init(VeertuType *owner, VeertuMemArea *system_memory,
                            VeertuMemArea *pci_address_space)
{
    /* Set to lower priority than RAM */
    mem_area_add_child_overlap(system_memory, 0x0,
                                        pci_address_space, -1);
}

void pc_acpi_init(const char *default_dsdt)
{
    char *filename;

    if (acpi_tables != NULL) {
        /* manually set via -acpitable, leave it alone */
        return;
    }

    filename = vmx_find_file(QEMU_FILE_TYPE_BIOS, default_dsdt);
    if (filename == NULL) {
        fprintf(stderr, "WARNING: failed to find %s\n", default_dsdt);
    } else {
        char *arg;
        QemuOpts *opts;
        Error *err = NULL;

        arg = g_strdup_printf("file=%s", filename);

        /* creates a deep copy of "arg" */
        opts = vmx_opts_parse(vmx_find_opts("acpi"), arg, 0);
        g_assert(opts != NULL);

        acpi_table_add_builtin(opts, &err);
        if (err) {
            error_report("WARNING: failed to load %s: %s", filename,
                         error_get_pretty(err));
            error_free(err);
        }
        g_free(arg);
        g_free(filename);
    }
}

FWCfgState *xen_load_linux(const char *kernel_filename,
                           const char *kernel_cmdline,
                           const char *initrd_filename,
                           ram_addr_t below_4g_mem_size,
                           PcGuestInfo *guest_info)
{
    int i;
    FWCfgState *fw_cfg;

    assert(kernel_filename != NULL);

    fw_cfg = fw_cfg_init_io(BIOS_CFG_IOPORT);
    rom_set_fw(fw_cfg);

    load_linux(fw_cfg, kernel_filename, initrd_filename,
               kernel_cmdline, below_4g_mem_size);
    for (i = 0; i < nb_option_roms; i++) {
        assert(!strcmp(option_rom[i].name, "linuxboot.bin") ||
               !strcmp(option_rom[i].name, "multiboot.bin"));
        rom_add_option(option_rom[i].name, option_rom[i].bootindex);
    }
    guest_info->fw_cfg = fw_cfg;
    return fw_cfg;
}

void memory_region_allocate_system_memory(VeertuMemArea *mr, VeertuType *owner,
                                           const char *name,
                                           uint64_t ram_size)
{
    if (mem_path) {
#ifdef __linux__
        Error *err = NULL;
        memory_region_init_ram_from_file(mr, owner, name, ram_size, false,
                                         mem_path, &err);
        
        /* Legacy behavior: if allocation failed, fall back to
         * regular RAM allocation.
         */
        if (err) {
            qerror_report_err(err);
            error_free(err);
            mem_area_init_ram(mr, name, ram_size, &error_abort);
        }
#else
        fprintf(stderr, "-mem-path not supported on this host\n");
        exit(1);
#endif
    } else {
        mem_area_init_ram(mr, name, ram_size, &error_abort);
    }
    vmstate_register_ram_global(mr);
}

FWCfgState *pc_memory_init(MachineState *machine,
                           VeertuMemArea *system_memory,
                           ram_addr_t below_4g_mem_size,
                           ram_addr_t above_4g_mem_size,
                           VeertuMemArea *rom_memory,
                           VeertuMemArea **ram_memory,
                           PcGuestInfo *guest_info)
{
    int linux_boot, i;
    VeertuMemArea *ram, *option_rom_mr;
    VeertuMemArea *ram_below_4g, *ram_above_4g;
    FWCfgState *fw_cfg;
    PCMachineState *pcms = PC_MACHINE(machine);

    assert(machine->ram_size == below_4g_mem_size + above_4g_mem_size);

    linux_boot = (machine->kernel_filename != NULL);

    /* Allocate RAM.  We allocate it as a single memory region and use
     * aliases to address portions of it, mostly for backwards compatibility
     * with older qemus that used vmx_ram_alloc().
     */
    ram = g_malloc(sizeof(*ram));
    memory_region_allocate_system_memory(ram, NULL, "pc.ram",
                                         machine->ram_size);
    *ram_memory = ram;
    ram_below_4g = g_malloc(sizeof(*ram_below_4g));
    mem_area_init_alias(ram_below_4g, "ram-below-4g", ram,
                             0, below_4g_mem_size);
    mem_area_add_child(system_memory, 0, ram_below_4g);
    e820_add_entry(0, below_4g_mem_size, E820_RAM);
    if (above_4g_mem_size > 0) {
        ram_above_4g = g_malloc(sizeof(*ram_above_4g));
        mem_area_init_alias(ram_above_4g,"ram-above-4g", ram,
                                 below_4g_mem_size, above_4g_mem_size);
        mem_area_add_child(system_memory, 0x100000000ULL,
                                    ram_above_4g);
        e820_add_entry(0x100000000ULL, above_4g_mem_size, E820_RAM);
    }

    if (!guest_info->has_reserved_memory &&
        (machine->ram_slots ||
         (machine->maxram_size > machine->ram_size))) {
        MachineClass *mc = MACHINE_GET_CLASS(machine);

        error_report("\"-memory 'slots|maxmem'\" is not supported by: %s",
                     mc->name);
        exit(EXIT_FAILURE);
    }

    /* initialize hotplug memory address space */
    if (guest_info->has_reserved_memory &&
        (machine->ram_size < machine->maxram_size)) {
        ram_addr_t hotplug_mem_size =
            machine->maxram_size - machine->ram_size;

        if (machine->ram_slots > ACPI_MAX_RAM_SLOTS) {
            error_report("unsupported amount of memory slots: %"PRIu64,
                         machine->ram_slots);
            exit(EXIT_FAILURE);
        }

        pcms->hotplug_memory_base =
            ROUND_UP(0x100000000ULL + above_4g_mem_size, 1ULL << 30);

        if (pcms->enforce_aligned_dimm) {
            /* size hotplug region assuming 1G page max alignment per slot */
            hotplug_mem_size += (1ULL << 30) * machine->ram_slots;
        }

        if ((pcms->hotplug_memory_base + hotplug_mem_size) <
            hotplug_mem_size) {
            error_report("unsupported amount of maximum memory: " RAM_ADDR_FMT,
                         machine->maxram_size);
            exit(EXIT_FAILURE);
        }

        memory_area_init(&pcms->hotplug_memory,
                           "hotplug-memory", hotplug_mem_size);
        mem_area_add_child(system_memory, pcms->hotplug_memory_base,
                                    &pcms->hotplug_memory);
    }

    /* Initialize PC system firmware */
    pc_system_firmware_init(rom_memory, guest_info->isapc_ram_fw);

    option_rom_mr = g_malloc(sizeof(*option_rom_mr));
    mem_area_init_ram(option_rom_mr, "pc.rom", PC_ROM_SIZE,
                           &error_abort);
    vmstate_register_ram_global(option_rom_mr);
    mem_area_add_child_overlap(rom_memory,
                                        PC_ROM_MIN_VGA,
                                        option_rom_mr,
                                        1);

    fw_cfg = bochs_bios_init();
    rom_set_fw(fw_cfg);

    if (guest_info->has_reserved_memory && pcms->hotplug_memory_base) {
        uint64_t *val = g_malloc(sizeof(*val));
        *val = cpu_to_le64(ROUND_UP(pcms->hotplug_memory_base, 0x1ULL << 30));
        fw_cfg_add_file(fw_cfg, "etc/reserved-memory-end", val, sizeof(*val));
    }

    if (linux_boot) {
        load_linux(fw_cfg, machine->kernel_filename, machine->initrd_filename,
                   machine->kernel_cmdline, below_4g_mem_size);
    }

    for (i = 0; i < nb_option_roms; i++) {
        rom_add_option(option_rom[i].name, option_rom[i].bootindex);
    }
    guest_info->fw_cfg = fw_cfg;
    return fw_cfg;
}

vmx_irq *pc_allocate_cpu_irq(void)
{
    return vmx_allocate_irqs(pic_irq_request, NULL, 1);
}

DeviceState *pc_vga_init(ISABus *isa_bus, PCIBus *pci_bus)
{
    DeviceState *dev = NULL;

    if (pci_bus) {
        PCIDevice *pcidev = pci_vga_init(pci_bus);
        dev = pcidev ? &pcidev->qdev : NULL;
    } else if (isa_bus) {
        ISADevice *isadev = isa_vga_init(isa_bus);
        dev = isadev ? DEVICE(isadev) : NULL;
    }
    return dev;
}

static void cpu_request_exit(void *opaque, int irq, int level)
{
    CPUState *cpu = current_cpu;

    if (cpu && level) {
        cpu_exit(cpu);
    }
}

static const MemAreaOps ioport80_io_ops = {
    .write = ioport80_write,
    .read = ioport80_read,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
};

static const MemAreaOps ioportF0_io_ops = {
    .write = ioportF0_write,
    .read = ioportF0_read,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
};

ISADevice *i8042;

void pc_basic_device_init(ISABus *isa_bus, vmx_irq *gsi,
                          ISADevice **rtc_state,
                          ISADevice **floppy,
                          bool no_vmport,
                          uint32_t hpet_irqs)
{
    int i;
    DeviceState *hpet = NULL;
    int pit_isa_irq = 0;
    vmx_irq pit_alt_irq = NULL;
    vmx_irq rtc_irq = NULL;
    vmx_irq *a20_line;
    ISADevice *port92, *vmmouse, *pit = NULL;
    vmx_irq *cpu_exit_irq;
    VeertuMemArea *ioport80_io = g_new(VeertuMemArea, 1);
    VeertuMemArea *ioportF0_io = g_new(VeertuMemArea, 1);

    memory_area_init_io(ioport80_io, NULL, &ioport80_io_ops, NULL, "ioport80", 1);
    mem_area_add_child(isa_bus->address_space_io, 0x80, ioport80_io);

    memory_area_init_io(ioportF0_io, NULL, &ioportF0_io_ops, NULL, "ioportF0", 1);
    mem_area_add_child(isa_bus->address_space_io, 0xf0, ioportF0_io);

    *rtc_state = rtc_init(isa_bus, 2000, rtc_irq);

    vmx_register_boot_set(pc_boot_set, *rtc_state);

    pit = pit_init(isa_bus, 0x40, pit_isa_irq, pit_alt_irq);

    pcspk_init(isa_bus, pit);
#if 0
    for(i = 0; i < MAX_SERIAL_PORTS; i++) {
        if (serial_hds[i]) {
            serial_isa_init(isa_bus, i, serial_hds[i]);
        }
    }
#endif
#if 0
    for(i = 0; i < MAX_PARALLEL_PORTS; i++) {
        if (parallel_hds[i]) {
            parallel_init(isa_bus, i, parallel_hds[i]);
        }
    }
#endif
    a20_line = vmx_allocate_irqs(handle_a20_line_change, first_cpu, 2);
    i8042 = isa_create_simple(isa_bus, "i8042");
    i8042_setup_a20_line(i8042, &a20_line[0]);
    if (!no_vmport) {
        vmport_init(isa_bus);
        vmmouse = isa_try_create(isa_bus, "vmmouse");
    } else {
        vmmouse = NULL;
    }
    if (vmmouse) {
        DeviceState *dev = DEVICE(vmmouse);
        VMMouseState *s = VMMOUSE(dev);
        s->ps2_mouse = i8042;
        qdev_init_nofail(dev);
    }

    isa_create_simple(isa_bus, "vmx_port");
    
    port92 = isa_create_simple(isa_bus, "port92");
    port92_init(port92, &a20_line[1]);

    cpu_exit_irq = vmx_allocate_irqs(cpu_request_exit, NULL, 1);
    DMA_init(0, cpu_exit_irq);
}

void pc_nic_init(ISABus *isa_bus, PCIBus *pci_bus)
{
    int i;

    for (i = 0; i < nb_nics; i++) {
        NICInfo *nd = &nd_table[i];

        if (!pci_bus || (nd->model && strcmp(nd->model, "ne2k_isa") == 0)) {
            pc_init_ne2k_isa(isa_bus, nd);
        } else {
            pci_nic_init_nofail(nd, pci_bus, "e1000", NULL);
        }
    }
}

void pc_pci_device_init(PCIBus *pci_bus)
{
    int max_bus;
    int bus;

    max_bus = drive_get_max_bus(IF_SCSI);
    for (bus = 0; bus <= max_bus; bus++) {
        pci_create_simple(pci_bus, -1, "lsi53c895a");
    }
}

void ioapic_init_gsi(GSIState *gsi_state, const char *parent_name)
{
    DeviceState *dev;
    SysBusDevice *d;
    unsigned int i;

    dev = qdev_create(NULL, "ioapic");
    qdev_init_nofail(dev);
    d = SYS_BUS_DEVICE(dev);
    sysbus_mmio_map(d, 0, IO_APIC_DEFAULT_ADDRESS);
}

extern MachineClass *default_machine;

static void pc_generic_machine_class_init(VeertuTypeClassHold *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    QEMUMachine *qm = data;

    mc->family = qm->family;
    mc->name = qm->name;
    mc->alias = qm->alias;
    mc->desc = qm->desc;
    mc->init = qm->init;
    mc->reset = qm->reset;
    mc->hot_add_cpu = qm->hot_add_cpu;
    mc->kvm_type = qm->kvm_type;
    mc->block_default_type = qm->block_default_type;
    mc->units_per_default_bus = qm->units_per_default_bus;
    mc->max_cpus = qm->max_cpus;
    mc->no_serial = qm->no_serial;
    mc->no_parallel = qm->no_parallel;
    mc->use_virtcon = qm->use_virtcon;
    mc->use_sclp = qm->use_sclp;
    mc->no_floppy = qm->no_floppy;
    mc->no_cdrom = qm->no_cdrom;
    mc->no_sdcard = qm->no_sdcard;
    mc->is_default = qm->is_default;
    mc->default_machine_opts = qm->default_machine_opts;
    mc->default_boot_order = qm->default_boot_order;
    mc->default_display = qm->default_display;
    mc->compat_props = qm->compat_props;
    mc->hw_version = qm->hw_version;
    
    if (mc->is_default) {
        default_machine = mc;
    }
}

void vmx_register_pc_machine(QEMUMachine *m)
{
    char *name = g_strconcat(m->name, TYPE_MACHINE_SUFFIX, NULL);
    VeertuTypeInfo ti = {
        .name       = name,
        .parent     = TYPE_PC_MACHINE,
        .class_init = pc_generic_machine_class_init,
        .class_data = (void *)m,
    };

    register_type_internal(&ti);
    g_free(name);
}

static int pc_dimm_count(VeertuType *obj, void *opaque)
{
    int *count = opaque;

    return 0;
}

static int pc_existing_dimms_capacity(VeertuType *obj, void *opaque)
{
    Error *local_err = NULL;
    uint64_t *size = opaque;

    return 0;
}

static void
pc_machine_get_hotplug_memory_region_size(VeertuType *obj, Visitor *v, void *opaque,
                                          const char *name, Error **errp)
{
    PCMachineState *pcms = PC_MACHINE(obj);
    int64_t value = mem_area_get_size(&pcms->hotplug_memory);

    visit_type_int(v, &value, name, errp);
}

static void pc_machine_get_max_ram_below_4g(VeertuType *obj, Visitor *v,
                                         void *opaque, const char *name,
                                         Error **errp)
{
    PCMachineState *pcms = PC_MACHINE(obj);
    uint64_t value = pcms->max_ram_below_4g;

    visit_type_size(v, &value, name, errp);
}

static void pc_machine_set_max_ram_below_4g(VeertuType *obj, Visitor *v,
                                         void *opaque, const char *name,
                                         Error **errp)
{
    PCMachineState *pcms = PC_MACHINE(obj);
    Error *error = NULL;
    uint64_t value;

    visit_type_size(v, &value, name, &error);
    if (error) {
        error_propagate(errp, error);
        return;
    }
    if (value > (1ULL << 32)) {
        error_set(&error, ERROR_CLASS_GENERIC_ERROR,
                  "Machine option 'max-ram-below-4g=%"PRIu64
                  "' expects size less than or equal to 4G", value);
        error_propagate(errp, error);
        return;
    }

    if (value < (1ULL << 20)) {
        error_report("Warning: small max_ram_below_4g(%"PRIu64
                     ") less than 1M.  BIOS may not work..",
                     value);
    }

    pcms->max_ram_below_4g = value;
}

static void pc_machine_get_vmport(VeertuType *obj, Visitor *v, void *opaque,
                                  const char *name, Error **errp)
{
    PCMachineState *pcms = PC_MACHINE(obj);
    OnOffAuto vmport = pcms->vmport;

    visit_type_OnOffAuto(v, &vmport, name, errp);
}

static void pc_machine_set_vmport(VeertuType *obj, Visitor *v, void *opaque,
                                  const char *name, Error **errp)
{
    PCMachineState *pcms = PC_MACHINE(obj);

    visit_type_OnOffAuto(v, &pcms->vmport, name, errp);
}

static bool pc_machine_get_aligned_dimm(VeertuType *obj, Error **errp)
{
    PCMachineState *pcms = PC_MACHINE(obj);

    return pcms->enforce_aligned_dimm;
}

static void pc_machine_initfn(VeertuType *obj)
{
    PCMachineState *pcms = PC_MACHINE(obj);


    pcms->max_ram_below_4g = 1ULL << 32; /* 4G */



    pcms->vmport = ON_OFF_AUTO_AUTO;


    pcms->enforce_aligned_dimm = true;

}

static void pc_machine_class_init(VeertuTypeClassHold *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PCMachineClass *pcmc = PC_MACHINE_CLASS(oc);
}

static const VeertuTypeInfo pc_machine_info = {
    .name = TYPE_PC_MACHINE,
    .parent = TYPE_MACHINE,
    .instance_size = sizeof(PCMachineState),
    .instance_init = pc_machine_initfn,
    .class_size = sizeof(PCMachineClass),
    .class_init = pc_machine_class_init,
};

void pc_machine_register_types(void)
{
    register_type_internal(&pc_machine_info);
}

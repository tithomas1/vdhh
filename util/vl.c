/*
 * QEMU System Emulator
 *
 * Copyright (c) 2003-2008 Fabrice Bellard
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
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>

#include "config-host.h"

#if defined(CONFIG_VDE)
#include <libvdeplug.h>
#endif

#ifdef CONFIG_SDL
#if defined(__APPLE__) || defined(main)
#include <SDL.h>
int vmx_main(const char* vmpath, int argc, char **argv, char **envp);
int main(int argc, char **argv)
{
    return vmx_main(NULL, argc, argv, NULL);
}
#undef main
#define main vmx_main
#endif
#endif /* CONFIG_SDL */

#ifdef CONFIG_COCOA
#undef main
#define main vmx_main
#endif /* CONFIG_COCOA */

#include <glib.h>

#include "qemu/sockets.h"
#include "hw.h"
#include "boards.h"
#include "emuaccel.h"
#include "usb.h"
#include "ipc.h"
#include "isa.h"
#include "ismbios.h"
#include "nqdev.h"
#include "loader.h"
#include "monitor/qdev.h"
#include "net/net.h"
#include "net/slirp.h"
#include "monitor/monitor.h"
#include "ui/console.h"
#include "sysemu.h"
#include "qemu/timer.h"
#include "emuchar.h"
#include "qemu/bitmap.h"
#include "emublockdev.h"
#include "emudma.h"
#include "audio/audio.h"
#include "migration.h"
#include "veertuemu.h"
#include "qapi/qmp/qjson.h"
#include "qemu/option.h"
#include "qemu/config-file.h"
#include "qemu-options.h"
#include "qmp-commands.h"
#include "qemu/main-loop.h"
#ifdef CONFIG_VIRTFS
#include "fsdev/qemu-fsdev.h"
#endif

#include "slirp/libslirp.h"

#include "qemu/queue.h"
#include "emucpus.h"
#include "emuarch_init.h"
#include "qemu/osdep.h"

#include "qapi/string-input-visitor.h"
#include "qapi/opts-visitor.h"
#include "qapi-event.h"
#include "vmlibrary_ops.h"

#define DEFAULT_RAM_SIZE 128

#define MAX_SCLP_CONSOLES 1

static const char *data_dir[16] = {NULL};
static int data_dir_idx = 0;
const char *bios_name = NULL;
enum vga_retrace_method vga_retrace_method = VGA_RETRACE_DUMB;
DisplayType display_type = DT_DEFAULT;
static int display_remote;
const char* keyboard_layout = NULL;
ram_addr_t ram_size;
const char *mem_path = NULL;
int mem_prealloc = 0; /* force preallocation of physical target memory */
bool enable_mlock = false;
int nb_nics;
NICInfo nd_table[MAX_NICS];
int autostart;
static int rtc_utc = 1;
static int rtc_date_offset = -1; /* -1 means no change */
QEMUClockType rtc_clock;
int vga_interface_type = VGA_NONE;
static int full_screen = 0;
static int no_frame = 0;
int no_quit = 0;
#ifdef CONFIG_GTK
static bool grab_on_hover;
#endif
CharDriverState *serial_hds[MAX_SERIAL_PORTS];
CharDriverState *parallel_hds[MAX_PARALLEL_PORTS];
CharDriverState *sclp_hds[MAX_SCLP_CONSOLES];
int win2k_install_hack = 0;
int singlestep = 0;
int smp_cpus = 1;
int max_cpus = 0;
int smp_cores = 1;
int smp_threads = 1;
int acpi_enabled = 1;
int no_hpet = 0;
int no_hyperv = 0;
int fd_bootchk = 1;
static int no_reboot;
int no_shutdown = 0;
int cursor_hide = 1;
int graphic_rotate = 0;
const char *watchdog;
QEMUOptionRom option_rom[MAX_OPTION_ROMS];
int nb_option_roms;
int semihosting_enabled = 0;
int old_param = 0;
const char *vmx_name;
int alt_grab = 0;
int ctrl_grab = 0;
unsigned int nb_prom_envs = 0;
const char *prom_envs[MAX_PROM_ENVS];
int boot_menu;
bool boot_strict;
uint8_t *boot_splash_filedata;
size_t boot_splash_filedata_size;
uint8_t vmx_extra_params_fw[2];
uint8_t vmx_extra_params_fw2[2];

int icount_align_option;

int nb_numa_nodes;
int max_numa_nodeid;
NodeInfo numa_info[MAX_NODES];

extern int slirp_used;

/* The bytes in vmx_uuid[] are in the order specified by RFC4122, _not_ in the
 * little-endian "wire format" described in the SMBIOS 2.6 specification.
 */
uint8_t vmx_uuid[16];
bool vmx_uuid_set;

static VeertuNotifiers exit_notifiers =
    VEERTU_NOTIFIERS_INIT(exit_notifiers);

static VeertuNotifiers machine_init_done_notifiers =
    VEERTU_NOTIFIERS_INIT(machine_init_done_notifiers);

static int has_defaults = 1;
static int default_serial = 1;
static int default_parallel = 1;
static int default_virtcon = 1;
static int default_sclp = 1;
static int default_monitor = 1;
static int default_floppy = 0;
static int default_cdrom = 1;
static int default_sdcard = 1;
static int default_vga = 1;

static struct {
    const char *driver;
    int *flag;
} default_list[] = {
    { .driver = "isa-serial",           .flag = &default_serial    },
    { .driver = "isa-parallel",         .flag = &default_parallel  },
    { .driver = "isa-fdc",              .flag = &default_floppy    },
    { .driver = "ide-cd",               .flag = &default_cdrom     },
    { .driver = "ide-hd",               .flag = &default_cdrom     },
    { .driver = "ide-drive",            .flag = &default_cdrom     },
    { .driver = "scsi-cd",              .flag = &default_cdrom     },
    { .driver = "VGA",                  .flag = &default_vga       },
    { .driver = "isa-vga",              .flag = &default_vga       },
    { .driver = "cirrus-vga",           .flag = &default_vga       },
    { .driver = "isa-cirrus-vga",       .flag = &default_vga       },
    { .driver = "vmware-svga",          .flag = &default_vga       },
    { .driver = "qxl-vga",              .flag = &default_vga       },
};

static QemuOptsList vmx_rtc_opts = {
    .name = "rtc",
    .head = QTAILQ_HEAD_INITIALIZER(vmx_rtc_opts.head),
    .desc = {
        {
            .name = "base",
            .type = QEMU_OPT_STRING,
        },{
            .name = "clock",
            .type = QEMU_OPT_STRING,
        },{
            .name = "driftfix",
            .type = QEMU_OPT_STRING,
        },
        { /* end of list */ }
    },
};

static QemuOptsList vmx_sandbox_opts = {
    .name = "sandbox",
    .implied_opt_name = "enable",
    .head = QTAILQ_HEAD_INITIALIZER(vmx_sandbox_opts.head),
    .desc = {
        {
            .name = "enable",
            .type = QEMU_OPT_BOOL,
        },
        { /* end of list */ }
    },
};

static QemuOptsList vmx_trace_opts = {
    .name = "trace",
    .implied_opt_name = "trace",
    .head = QTAILQ_HEAD_INITIALIZER(vmx_trace_opts.head),
    .desc = {
        {
            .name = "events",
            .type = QEMU_OPT_STRING,
        },{
            .name = "file",
            .type = QEMU_OPT_STRING,
        },
        { /* end of list */ }
    },
};

static QemuOptsList vmx_option_rom_opts = {
    .name = "option-rom",
    .implied_opt_name = "romfile",
    .head = QTAILQ_HEAD_INITIALIZER(vmx_option_rom_opts.head),
    .desc = {
        {
            .name = "bootindex",
            .type = QEMU_OPT_NUMBER,
        }, {
            .name = "romfile",
            .type = QEMU_OPT_STRING,
        },
        { /* end of list */ }
    },
};

static QemuOptsList vmx_machine_opts = {
    .name = "machine",
    .implied_opt_name = "type",
    .merge_lists = true,
    .head = QTAILQ_HEAD_INITIALIZER(vmx_machine_opts.head),
    .desc = {
        /*
         * no elements => accept any
         * sanity checking will happen later
         * when setting machine properties
         */
        { }
    },
};

static QemuOptsList vmx_boot_opts = {
    .name = "boot-opts",
    .implied_opt_name = "order",
    .merge_lists = true,
    .head = QTAILQ_HEAD_INITIALIZER(vmx_boot_opts.head),
    .desc = {
        {
            .name = "key",
            .type = QEMU_OPT_STRING,
        }, {
            .name = "order",
            .type = QEMU_OPT_STRING,
        }, {
            .name = "order",
            .type = QEMU_OPT_STRING,
        }, {
            .name = "once",
            .type = QEMU_OPT_STRING,
        }, {
            .name = "menu",
            .type = QEMU_OPT_BOOL,
        }, {
            .name = "splash",
            .type = QEMU_OPT_STRING,
        }, {
            .name = "splash-time",
            .type = QEMU_OPT_STRING,
        }, {
            .name = "reboot-timeout",
            .type = QEMU_OPT_STRING,
        }, {
            .name = "strict",
            .type = QEMU_OPT_BOOL,
        },
        { /*End of list */ }
    },
};

static QemuOptsList vmx_add_fd_opts = {
    .name = "add-fd",
    .head = QTAILQ_HEAD_INITIALIZER(vmx_add_fd_opts.head),
    .desc = {
        {
            .name = "fd",
            .type = QEMU_OPT_NUMBER,
            .help = "file descriptor of which a duplicate is added to fd set",
        },{
            .name = "set",
            .type = QEMU_OPT_NUMBER,
            .help = "ID of the fd set to add fd to",
        },{
            .name = "opaque",
            .type = QEMU_OPT_STRING,
            .help = "free-form string used to describe fd",
        },
        { /* end of list */ }
    },
};

static QemuOptsList vmx_object_opts = {
    .name = "object",
    .implied_opt_name = "qom-type",
    .head = QTAILQ_HEAD_INITIALIZER(vmx_object_opts.head),
    .desc = {
        { }
    },
};

static QemuOptsList vmx_tpmdev_opts = {
    .name = "tpmdev",
    .implied_opt_name = "type",
    .head = QTAILQ_HEAD_INITIALIZER(vmx_tpmdev_opts.head),
    .desc = {
        /* options are defined in the TPM backends */
        { /* end of list */ }
    },
};

static QemuOptsList vmx_realtime_opts = {
    .name = "realtime",
    .head = QTAILQ_HEAD_INITIALIZER(vmx_realtime_opts.head),
    .desc = {
        {
            .name = "mlock",
            .type = QEMU_OPT_BOOL,
        },
        { /* end of list */ }
    },
};

static QemuOptsList vmx_msg_opts = {
    .name = "msg",
    .head = QTAILQ_HEAD_INITIALIZER(vmx_msg_opts.head),
    .desc = {
        {
            .name = "timestamp",
            .type = QEMU_OPT_BOOL,
        },
        { /* end of list */ }
    },
};

static QemuOptsList vmx_mem_opts = {
    .name = "memory",
    .implied_opt_name = "size",
    .head = QTAILQ_HEAD_INITIALIZER(vmx_mem_opts.head),
    .merge_lists = true,
    .desc = {
        {
            .name = "size",
            .type = QEMU_OPT_SIZE,
        },
        {
            .name = "slots",
            .type = QEMU_OPT_NUMBER,
        },
        {
            .name = "maxmem",
            .type = QEMU_OPT_SIZE,
        },
        { /* end of list */ }
    },
};

static QemuOptsList vmx_icount_opts = {
    .name = "icount",
    .implied_opt_name = "shift",
    .merge_lists = true,
    .head = QTAILQ_HEAD_INITIALIZER(vmx_icount_opts.head),
    .desc = {
        {
            .name = "shift",
            .type = QEMU_OPT_STRING,
        }, {
            .name = "align",
            .type = QEMU_OPT_BOOL,
        },
        { /* end of list */ }
    },
};

/**
 * Get machine options
 *
 * Returns: machine options (never null).
 */
QemuOpts *vmx_get_machine_opts(void)
{
    return vmx_find_opts_singleton("machine");
}

const char *vmx_get_vm_name(void)
{
    return vmx_name;
}

static void res_free(void)
{
    if (boot_splash_filedata != NULL) {
        g_free(boot_splash_filedata);
        boot_splash_filedata = NULL;
    }
}

static int default_driver_check(QemuOpts *opts, void *opaque)
{
    const char *driver = vmx_opt_get(opts, "driver");
    int i;

    if (!driver)
        return 0;
    for (i = 0; i < ARRAY_SIZE(default_list); i++) {
        if (strcmp(default_list[i].driver, driver) != 0)
            continue;
        *(default_list[i].flag) = 0;
    }
    return 0;
}

/***********************************************************/
/* QEMU state */

static RunState current_run_state = RUN_STATE_PRELAUNCH;

/* We use RUN_STATE_MAX but any invalid value will do */
static RunState vmstop_requested = RUN_STATE_MAX;
static QemuMutex vmstop_lock;

typedef struct {
    RunState from;
    RunState to;
} RunStateTransition;

static const RunStateTransition runstate_transitions_def[] = {
    /*     from      ->     to      */
    { RUN_STATE_DEBUG, RUN_STATE_RUNNING },
    { RUN_STATE_DEBUG, RUN_STATE_FINISH_MIGRATE },

    { RUN_STATE_INMIGRATE, RUN_STATE_RUNNING },
    { RUN_STATE_INMIGRATE, RUN_STATE_PAUSED },

    { RUN_STATE_INTERNAL_ERROR, RUN_STATE_PAUSED },
    { RUN_STATE_INTERNAL_ERROR, RUN_STATE_FINISH_MIGRATE },

    { RUN_STATE_IO_ERROR, RUN_STATE_RUNNING },
    { RUN_STATE_IO_ERROR, RUN_STATE_FINISH_MIGRATE },

    { RUN_STATE_PAUSED, RUN_STATE_RUNNING },
    { RUN_STATE_PAUSED, RUN_STATE_FINISH_MIGRATE },

    { RUN_STATE_POSTMIGRATE, RUN_STATE_RUNNING },
    { RUN_STATE_POSTMIGRATE, RUN_STATE_FINISH_MIGRATE },

    { RUN_STATE_PRELAUNCH, RUN_STATE_RUNNING },
    { RUN_STATE_PRELAUNCH, RUN_STATE_FINISH_MIGRATE },
    { RUN_STATE_PRELAUNCH, RUN_STATE_INMIGRATE },

    { RUN_STATE_FINISH_MIGRATE, RUN_STATE_RUNNING },
    { RUN_STATE_FINISH_MIGRATE, RUN_STATE_POSTMIGRATE },

    { RUN_STATE_RESTORE_VM, RUN_STATE_RUNNING },

    { RUN_STATE_RUNNING, RUN_STATE_DEBUG },
    { RUN_STATE_RUNNING, RUN_STATE_INTERNAL_ERROR },
    { RUN_STATE_RUNNING, RUN_STATE_IO_ERROR },
    { RUN_STATE_RUNNING, RUN_STATE_PAUSED },
    { RUN_STATE_RUNNING, RUN_STATE_FINISH_MIGRATE },
    { RUN_STATE_RUNNING, RUN_STATE_RESTORE_VM },
    { RUN_STATE_RUNNING, RUN_STATE_SAVE_VM },
    { RUN_STATE_RUNNING, RUN_STATE_SHUTDOWN },
    { RUN_STATE_RUNNING, RUN_STATE_WATCHDOG },
    { RUN_STATE_RUNNING, RUN_STATE_GUEST_PANICKED },

    { RUN_STATE_SAVE_VM, RUN_STATE_RUNNING },

    { RUN_STATE_SHUTDOWN, RUN_STATE_PAUSED },
    { RUN_STATE_SHUTDOWN, RUN_STATE_FINISH_MIGRATE },
    { RUN_STATE_SHUTDOWN, RUN_STATE_RUNNING },

    { RUN_STATE_DEBUG, RUN_STATE_SUSPENDED },
    { RUN_STATE_RUNNING, RUN_STATE_SUSPENDED },
    { RUN_STATE_SUSPENDED, RUN_STATE_RUNNING },
    { RUN_STATE_SUSPENDED, RUN_STATE_FINISH_MIGRATE },

    { RUN_STATE_WATCHDOG, RUN_STATE_RUNNING },
    { RUN_STATE_WATCHDOG, RUN_STATE_FINISH_MIGRATE },

    { RUN_STATE_GUEST_PANICKED, RUN_STATE_RUNNING },
    { RUN_STATE_GUEST_PANICKED, RUN_STATE_FINISH_MIGRATE },

    { RUN_STATE_MAX, RUN_STATE_MAX },
};

static bool runstate_valid_transitions[RUN_STATE_MAX][RUN_STATE_MAX];

bool runstate_check(RunState state)
{
    return current_run_state == state;
}

static void runstate_init(void)
{
    const RunStateTransition *p;

    memset(&runstate_valid_transitions, 0, sizeof(runstate_valid_transitions));
    for (p = &runstate_transitions_def[0]; p->from != RUN_STATE_MAX; p++) {
        runstate_valid_transitions[p->from][p->to] = true;
    }

    vmx_mutex_init(&vmstop_lock);
}

/* This function will abort() on invalid state transitions */
void runstate_set(RunState new_state)
{
    assert(new_state < RUN_STATE_MAX);

    if (!runstate_valid_transitions[current_run_state][new_state]) {
        fprintf(stderr, "ERROR: invalid runstate transition: '%s' -> '%s'\n",
                RunState_lookup[current_run_state],
                RunState_lookup[new_state]);
        abort();
    }
    current_run_state = new_state;
}

int runstate_is_running(void)
{
    return runstate_check(RUN_STATE_RUNNING);
}

bool runstate_needs_reset(void)
{
    return runstate_check(RUN_STATE_INTERNAL_ERROR) ||
        runstate_check(RUN_STATE_SHUTDOWN);
}

StatusInfo *qmp_query_status(Error **errp)
{
    StatusInfo *info = g_malloc0(sizeof(*info));

    info->running = runstate_is_running();
    info->singlestep = singlestep;
    info->status = current_run_state;

    return info;
}

static bool vmx_vmstop_requested(RunState *r)
{
    vmx_mutex_lock(&vmstop_lock);
    *r = vmstop_requested;
    vmstop_requested = RUN_STATE_MAX;
    vmx_mutex_unlock(&vmstop_lock);
    return *r < RUN_STATE_MAX;
}

void vmx_system_vmstop_request_prepare(void)
{
    vmx_mutex_lock(&vmstop_lock);
}

void vmx_system_vmstop_request(RunState state)
{
    vmstop_requested = state;
    vmx_mutex_unlock(&vmstop_lock);
    vmx_notify_event();
}

void vm_start(void)
{
    RunState requested;

    vmx_vmstop_requested(&requested);
    if (runstate_is_running() && requested == RUN_STATE_MAX) {
        return;
    }

    /* Ensure that a STOP/RESUME pair of events is emitted if a
     * vmstop request was pending.  The BLOCK_IO_ERROR event, for
     * example, according to documentation is always followed by
     * the STOP event.
     */
    if (runstate_is_running()) {
        qapi_event_send_stop(&error_abort);
    } else {
        cpu_enable_ticks();
        runstate_set(RUN_STATE_RUNNING);
        vm_state_notify(1, RUN_STATE_RUNNING);
        resume_all_vcpus();
    }

    qapi_event_send_resume(&error_abort);
}


/***********************************************************/
/* real time host monotonic timer */

/***********************************************************/
/* host time/date access */
void vmx_get_timedate(struct tm *tm, int offset)
{
    time_t ti;

    time(&ti);
    ti += offset;
    if (rtc_date_offset == -1) {
        if (rtc_utc)
            gmtime_r(&ti, tm);
        else
            localtime_r(&ti, tm);
    } else {
        ti -= rtc_date_offset;
        gmtime_r(&ti, tm);
    }
}

int vmx_timedate_diff(struct tm *tm)
{
    time_t seconds;

    if (rtc_date_offset == -1)
        if (rtc_utc)
            seconds = mktimegm(tm);
        else {
            struct tm tmp = *tm;
            tmp.tm_isdst = -1; /* use timezone to figure it out */
            seconds = mktime(&tmp);
	}
    else
        seconds = mktimegm(tm) + rtc_date_offset;

    return seconds - time(NULL);
}

static void configure_rtc_date_offset(const char *startdate, int legacy)
{
    time_t rtc_start_date;
    struct tm tm;

    if (!strcmp(startdate, "now") && legacy) {
        rtc_date_offset = -1;
    } else {
        if (sscanf(startdate, "%d-%d-%dT%d:%d:%d",
                   &tm.tm_year,
                   &tm.tm_mon,
                   &tm.tm_mday,
                   &tm.tm_hour,
                   &tm.tm_min,
                   &tm.tm_sec) == 6) {
            /* OK */
        } else if (sscanf(startdate, "%d-%d-%d",
                          &tm.tm_year,
                          &tm.tm_mon,
                          &tm.tm_mday) == 3) {
            tm.tm_hour = 0;
            tm.tm_min = 0;
            tm.tm_sec = 0;
        } else {
            goto date_fail;
        }
        tm.tm_year -= 1900;
        tm.tm_mon--;
        rtc_start_date = mktimegm(&tm);
        if (rtc_start_date == -1) {
        date_fail:
            fprintf(stderr, "Invalid date format. Valid formats are:\n"
                            "'2006-06-17T16:01:21' or '2006-06-17'\n");
            exit(1);
        }
        rtc_date_offset = time(NULL) - rtc_start_date;
    }
}

static void configure_rtc(QemuOpts *opts)
{
    const char *value;

    value = vmx_opt_get(opts, "base");
    if (value) {
        if (!strcmp(value, "utc")) {
            rtc_utc = 1;
        } else if (!strcmp(value, "localtime")) {
            rtc_utc = 0;
        } else {
            configure_rtc_date_offset(value, 0);
        }
    }
    value = vmx_opt_get(opts, "clock");
    if (value) {
        if (!strcmp(value, "host")) {
            rtc_clock = QEMU_CLOCK_HOST;
        } else if (!strcmp(value, "rt")) {
            rtc_clock = QEMU_CLOCK_REALTIME;
        } else if (!strcmp(value, "vm")) {
            rtc_clock = QEMU_CLOCK_VIRTUAL;
        } else {
            fprintf(stderr, "veertu: invalid option value '%s'\n", value);
            exit(1);
        }
    }
    value = vmx_opt_get(opts, "driftfix");
    if (value) {
        if (!strcmp(value, "slew")) {
            static GlobalProperty slew_lost_ticks[] = {
                {
                    .driver   = "mc146818rtc",
                    .property = "lost_tick_policy",
                    .value    = "slew",
                },
                { /* end of list */ }
            };

        } else if (!strcmp(value, "none")) {
            /* discard is default */
        } else {
            fprintf(stderr, "veertu: invalid option value '%s'\n", value);
            exit(1);
        }
    }
}

static int parse_sandbox(QemuOpts *opts, void *opaque)
{
    return 0;
}

static int parse_name(QemuOpts *opts, void *opaque)
{
    const char *proc_name;

    if (vmx_opt_get(opts, "debug-threads")) {
        vmx_thread_naming(vmx_opt_get_bool(opts, "debug-threads", false));
    }
    vmx_name = vmx_opt_get(opts, "guest");

    proc_name = vmx_opt_get(opts, "process");
    if (proc_name) {
        os_set_proc_name(proc_name);
    }

    return 0;
}

bool defaults_enabled(void)
{
    return has_defaults;
}

bool usb_enabled(void)
{
    return machine_usb(current_machine);
}

#ifndef _WIN32
static int parse_add_fd(QemuOpts *opts, void *opaque)
{
    int fd, dupfd, flags;
    int64_t fdset_id;
    const char *fd_opaque = NULL;

    fd = vmx_opt_get_number(opts, "fd", -1);
    fdset_id = vmx_opt_get_number(opts, "set", -1);
    fd_opaque = vmx_opt_get(opts, "opaque");

    if (fd < 0) {
        qerror_report(ERROR_CLASS_GENERIC_ERROR,
                      "fd option is required and must be non-negative");
        return -1;
    }

    if (fd <= STDERR_FILENO) {
        qerror_report(ERROR_CLASS_GENERIC_ERROR,
                      "fd cannot be a standard I/O stream");
        return -1;
    }

    /*
     * All fds inherited across exec() necessarily have FD_CLOEXEC
     * clear, while qemu sets FD_CLOEXEC on all other fds used internally.
     */
    flags = fcntl(fd, F_GETFD);
    if (flags == -1 || (flags & FD_CLOEXEC)) {
        qerror_report(ERROR_CLASS_GENERIC_ERROR,
                      "fd is not valid or already in use");
        return -1;
    }

    if (fdset_id < 0) {
        qerror_report(ERROR_CLASS_GENERIC_ERROR,
                      "set option is required and must be non-negative");
        return -1;
    }

#ifdef F_DUPFD_CLOEXEC
    dupfd = fcntl(fd, F_DUPFD_CLOEXEC, 0);
#else
    dupfd = dup(fd);
    if (dupfd != -1) {
        vmx_set_cloexec(dupfd);
    }
#endif
    if (dupfd == -1) {
        qerror_report(ERROR_CLASS_GENERIC_ERROR,
                      "Error duplicating fd: %s", strerror(errno));
        return -1;
    }

    /* add the duplicate fd, and optionally the opaque string, to the fd set */
    monitor_fdset_add_fd(dupfd, true, fdset_id, fd_opaque ? true : false,
                         fd_opaque, NULL);

    return 0;
}

static int cleanup_add_fd(QemuOpts *opts, void *opaque)
{
    int fd;

    fd = vmx_opt_get_number(opts, "fd", -1);
    close(fd);

    return 0;
}
#endif

/***********************************************************/
/* QEMU Block devices */

#define HD_OPTS "media=disk"
#define CDROM_OPTS "media=cdrom"
#define FD_OPTS ""
#define PFLASH_OPTS ""
#define MTD_OPTS ""
#define SD_OPTS ""

static int drive_init_func(QemuOpts *opts, void *opaque)
{
    BlockInterfaceType *block_default_type = opaque;

    return drive_new(opts, *block_default_type) == NULL;
}

static int drive_enable_snapshot(QemuOpts *opts, void *opaque)
{
    if (vmx_opt_get(opts, "snapshot") == NULL) {
        vmx_opt_set(opts, "snapshot", "on");
    }
    return 0;
}

static void default_drive(int enable, int snapshot, BlockInterfaceType type,
                          int index, const char *optstr)
{
    QemuOpts *opts;
    DriveInfo *dinfo;

    if (!enable || drive_get_by_index(type, index)) {
        return;
    }

    opts = drive_add(type, index, NULL, optstr);
    if (snapshot) {
        drive_enable_snapshot(opts, NULL);
    }

    dinfo = drive_new(opts, type);
    if (!dinfo) {
        exit(1);
    }
    dinfo->is_default = true;

}

static QemuOptsList vmx_smp_opts = {
    .name = "smp-opts",
    .implied_opt_name = "cpus",
    .merge_lists = true,
    .head = QTAILQ_HEAD_INITIALIZER(vmx_smp_opts.head),
    .desc = {
        {
            .name = "cpus",
            .type = QEMU_OPT_NUMBER,
        }, {
            .name = "sockets",
            .type = QEMU_OPT_NUMBER,
        }, {
            .name = "cores",
            .type = QEMU_OPT_NUMBER,
        }, {
            .name = "threads",
            .type = QEMU_OPT_NUMBER,
        }, {
            .name = "maxcpus",
            .type = QEMU_OPT_NUMBER,
        },
        { /*End of list */ }
    },
};

static void smp_parse(QemuOpts *opts)
{
    if (opts) {

        unsigned cpus    = vmx_opt_get_number(opts, "cpus", 0);
        unsigned sockets = vmx_opt_get_number(opts, "sockets", 0);
        unsigned cores   = vmx_opt_get_number(opts, "cores", 0);
        unsigned threads = vmx_opt_get_number(opts, "threads", 0);

        /* compute missing values, prefer sockets over cores over threads */
        if (cpus == 0 || sockets == 0) {
            sockets = sockets > 0 ? sockets : 1;
            cores = cores > 0 ? cores : 1;
            threads = threads > 0 ? threads : 1;
            if (cpus == 0) {
                cpus = cores * threads * sockets;
            }
        } else if (cores == 0) {
            threads = threads > 0 ? threads : 1;
            cores = cpus / (sockets * threads);
        } else if (threads == 0) {
            threads = cpus / (cores * sockets);
        } else if (sockets * cores * threads < cpus) {
            fprintf(stderr, "cpu topology: error: "
                    "sockets (%u) * cores (%u) * threads (%u) < "
                    "smp_cpus (%u)\n",
                    sockets, cores, threads, cpus);
            exit(1);
        }

        max_cpus = vmx_opt_get_number(opts, "maxcpus", 0);

        smp_cpus = cpus;
        smp_cores = cores > 0 ? cores : 1;
        smp_threads = threads > 0 ? threads : 1;

    }

    if (max_cpus == 0) {
        max_cpus = smp_cpus;
    }

    if (max_cpus > MAX_CPUMASK_BITS) {
        fprintf(stderr, "Unsupported number of maxcpus\n");
        exit(1);
    }
    if (max_cpus < smp_cpus) {
        fprintf(stderr, "maxcpus must be equal to or greater than smp\n");
        exit(1);
    }

}

static void realtime_init(void)
{
    if (enable_mlock) {
        if (os_mlock() < 0) {
            fprintf(stderr, "veertu: locking memory failed\n");
            exit(1);
        }
    }
}


static void configure_msg(QemuOpts *opts)
{
}

/***********************************************************/
/* USB devices */

static int usb_device_add(const char *devname)
{
    USBDevice *dev = NULL;
#ifndef CONFIG_LINUX
    const char *p;
#endif

    if (!usb_enabled()) {
        return -1;
    }

    /* drivers with .usbdevice_name entry in USBDeviceInfo */
    dev = usbdevice_create(devname);
    if (dev)
        goto done;

    /* the other ones */
#ifndef CONFIG_LINUX
    /* only the linux version is qdev-ified, usb-bsd still needs this */
    if (strstart(devname, "host:", &p)) {
        dev = usb_host_device_open(usb_bus_find(-1), p);
    }
#endif
    if (!dev)
        return -1;

done:
    return 0;
}

static int usb_device_del(const char *devname)
{
    int bus_num, addr;
    const char *p;

    if (strstart(devname, "host:", &p)) {
        return -1;
    }

    if (!usb_enabled()) {
        return -1;
    }

    p = strchr(devname, '.');
    if (!p)
        return -1;
    bus_num = strtoul(devname, NULL, 0);
    addr = strtoul(p + 1, NULL, 0);

    return usb_device_delete_addr(bus_num, addr);
}

static int usb_parse(const char *cmdline)
{
    int r;
    r = usb_device_add(cmdline);
    if (r < 0) {
        fprintf(stderr, "veertu: could not add USB device '%s'\n", cmdline);
    }
    return r;
}

void do_usb_add(Monitor *mon, const QDict *qdict)
{
    const char *devname = qdict_get_str(qdict, "devname");
    if (usb_device_add(devname) < 0) {
        error_report("could not add USB device '%s'", devname);
    }
}

void do_usb_del(Monitor *mon, const QDict *qdict)
{
    const char *devname = qdict_get_str(qdict, "devname");
    if (usb_device_del(devname) < 0) {
        error_report("could not delete USB device '%s'", devname);
    }
}

/***********************************************************/
/* machine registration */

MachineState *current_machine;

static void machine_class_init(VeertuTypeClassHold *oc, void *data)
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
    mc->has_dynamic_sysbus = qm->has_dynamic_sysbus;
    mc->is_default = qm->is_default;
    mc->default_machine_opts = qm->default_machine_opts;
    mc->default_boot_order = qm->default_boot_order;
    mc->default_display = qm->default_display;
    mc->compat_props = qm->compat_props;
    mc->hw_version = qm->hw_version;
}

int vmx_register_machine(QEMUMachine *m)
{
    char *name = g_strconcat(m->name, TYPE_MACHINE_SUFFIX, NULL);
    VeertuTypeInfo ti = {
        .name       = name,
        .parent     = TYPE_MACHINE,
        .class_init = machine_class_init,
        .class_data = (void *)m,
    };

    register_type_internal(&ti);
    g_free(name);

    return 0;
}

static MachineClass *find_machine(const char *name)
{
    return NULL;
}

MachineClass *default_machine;

MachineClass *find_default_machine(void)
{
    global_init_type();
    return default_machine;
}

MachineInfoList *qmp_query_machines(Error **errp)
{
    return NULL;
}

static int machine_help_func(QemuOpts *opts, MachineState *machine)
{
    return 0;
}

/***********************************************************/
/* main execution loop */

struct vm_change_state_entry {
    VMChangeStateHandler *cb;
    void *opaque;
    QLIST_ENTRY (vm_change_state_entry) entries;
};

static QLIST_HEAD(vm_change_state_head, vm_change_state_entry) vm_change_state_head;

VMChangeStateEntry *vmx_add_vm_change_state_handler(VMChangeStateHandler *cb,
                                                     void *opaque)
{
    VMChangeStateEntry *e;

    e = g_malloc0(sizeof (*e));

    e->cb = cb;
    e->opaque = opaque;
    QLIST_INSERT_HEAD(&vm_change_state_head, e, entries);
    return e;
}

void vmx_del_vm_change_state_handler(VMChangeStateEntry *e)
{
    QLIST_REMOVE (e, entries);
    g_free (e);
}

void vm_state_notify(int running, RunState state)
{
    VMChangeStateEntry *e, *next;

    QLIST_FOREACH_SAFE(e, &vm_change_state_head, entries, next) {
        e->cb(e->opaque, running, state);
    }
}

/* reset/shutdown handler */

typedef struct QEMUResetEntry {
    QTAILQ_ENTRY(QEMUResetEntry) entry;
    QEMUResetHandler *func;
    void *opaque;
} QEMUResetEntry;

static QTAILQ_HEAD(reset_handlers, QEMUResetEntry) reset_handlers =
    QTAILQ_HEAD_INITIALIZER(reset_handlers);
static int reset_requested;
static int shutdown_requested, shutdown_signal = -1;
static pid_t shutdown_pid;
static int powerdown_requested;
static int debug_requested;
static int suspend_requested;
static WakeupReason wakeup_reason;
static VeertuNotifiers powerdown_notifiers =
    VEERTU_NOTIFIERS_INIT(powerdown_notifiers);
static VeertuNotifiers suspend_notifiers =
    VEERTU_NOTIFIERS_INIT(suspend_notifiers);
static VeertuNotifiers wakeup_notifiers =
    VEERTU_NOTIFIERS_INIT(wakeup_notifiers);
static uint32_t wakeup_reason_mask = ~(1 << QEMU_WAKEUP_REASON_NONE);

int vmx_shutdown_requested_get(void)
{
    return shutdown_requested;
}

int vmx_reset_requested_get(void)
{
    return reset_requested;
}

static int vmx_shutdown_requested(void)
{
    return atomic_xchg(&shutdown_requested, 0);
}

static void vmx_kill_report(void)
{
    if (shutdown_signal != -1) {
        fprintf(stderr, "veertu: terminating on signal %d", shutdown_signal);
        if (shutdown_pid == 0) {
            /* This happens for eg ^C at the terminal, so it's worth
             * avoiding printing an odd message in that case.
             */
            fputc('\n', stderr);
        } else {
            fprintf(stderr, " from pid " FMT_pid "\n", shutdown_pid);
        }
        shutdown_signal = -1;
    }
}

static int vmx_reset_requested(void)
{
    int r = reset_requested;
    reset_requested = 0;
    return r;
}

static int vmx_suspend_requested(void)
{
    int r = suspend_requested;
    suspend_requested = 0;
    return r;
}

static WakeupReason vmx_wakeup_requested(void)
{
    return wakeup_reason;
}

static int vmx_powerdown_requested(void)
{
    int r = powerdown_requested;
    powerdown_requested = 0;
    return r;
}

static int vmx_debug_requested(void)
{
    int r = debug_requested;
    debug_requested = 0;
    return r;
}

void vmx_register_reset(QEMUResetHandler *func, void *opaque)
{
    QEMUResetEntry *re = g_malloc0(sizeof(QEMUResetEntry));

    re->func = func;
    re->opaque = opaque;
    QTAILQ_INSERT_TAIL(&reset_handlers, re, entry);
}

void vmx_unregister_reset(QEMUResetHandler *func, void *opaque)
{
    QEMUResetEntry *re;

    QTAILQ_FOREACH(re, &reset_handlers, entry) {
        if (re->func == func && re->opaque == opaque) {
            QTAILQ_REMOVE(&reset_handlers, re, entry);
            g_free(re);
            return;
        }
    }
}

void vmx_devices_reset(void)
{
    QEMUResetEntry *re, *nre;

    /* reset all devices */
    QTAILQ_FOREACH_SAFE(re, &reset_handlers, entry, nre) {
        re->func(re->opaque);
    }
}

void vmx_system_reset(bool report)
{
    MachineClass *mc;

    mc = current_machine ? MACHINE_GET_CLASS(current_machine) : NULL;

    if (mc && mc->reset) {
        mc->reset();
    } else {
        vmx_devices_reset();
    }
    if (report) {
        qapi_event_send_reset(&error_abort);
    }
    cpu_synchronize_all_post_reset();
}

void vmx_system_reset_request(void)
{
    if (no_reboot) {
        shutdown_requested = 1;
    } else {
        reset_requested = 1;
    }
    cpu_stop_current();
    vmx_notify_event();
}

static void vmx_system_suspend(void)
{
    pause_all_vcpus();
    veertu_notifiers_notify(&suspend_notifiers, NULL);
    runstate_set(RUN_STATE_SUSPENDED);
    qapi_event_send_suspend(&error_abort);
}

void vmx_system_suspend_request(void)
{
    if (runstate_check(RUN_STATE_SUSPENDED)) {
        return;
    }
    suspend_requested = 1;
    cpu_stop_current();
    vmx_notify_event();
}

void vmx_register_suspend_notifier(Notifier *notifier)
{
    veertu_notifiers_add(&suspend_notifiers, notifier);
}

void vmx_system_wakeup_request(WakeupReason reason)
{
    if (!runstate_check(RUN_STATE_SUSPENDED)) {
        return;
    }
    if (!(wakeup_reason_mask & (1 << reason))) {
        return;
    }
    runstate_set(RUN_STATE_RUNNING);
    wakeup_reason = reason;
    vmx_notify_event();
}

void vmx_system_wakeup_enable(WakeupReason reason, bool enabled)
{
    if (enabled) {
        wakeup_reason_mask |= (1 << reason);
    } else {
        wakeup_reason_mask &= ~(1 << reason);
    }
}

void vmx_register_wakeup_notifier(Notifier *notifier)
{
    veertu_notifiers_add(&wakeup_notifiers, notifier);
}

void vmx_system_killed(int signal, pid_t pid)
{
    shutdown_signal = signal;
    shutdown_pid = pid;
    no_shutdown = 0;
    vmx_system_shutdown_request();
}

void vmx_system_shutdown_request(void)
{
    shutdown_requested = 1;
    vmx_notify_event();
}

static void vmx_system_powerdown(void)
{
    qapi_event_send_powerdown(&error_abort);
    veertu_notifiers_notify(&powerdown_notifiers, NULL);
}

void vmx_system_powerdown_request(void)
{
    powerdown_requested = 1;
    vmx_notify_event();
}

void vmx_register_powerdown_notifier(Notifier *notifier)
{
    veertu_notifiers_add(&powerdown_notifiers, notifier);
}

void vmx_system_debug_request(void)
{
    debug_requested = 1;
    vmx_notify_event();
}

static bool main_loop_should_exit(void)
{
    RunState r;
    if (vmx_debug_requested()) {
        vm_stop(RUN_STATE_DEBUG);
    }
    if (vmx_suspend_requested()) {
        vmx_system_suspend();
    }
    if (vmx_shutdown_requested()) {
        vmx_kill_report();
        qapi_event_send_shutdown(&error_abort);
        if (no_shutdown) {
            vm_stop(RUN_STATE_SHUTDOWN);
        } else {
            return true;
        }
    }
    if (vmx_reset_requested()) {
        pause_all_vcpus();
        cpu_synchronize_all_states();
        vmx_system_reset(VMRESET_REPORT);
        resume_all_vcpus();
        if (runstate_needs_reset()) {
            runstate_set(RUN_STATE_PAUSED);
        }
    }
    if (vmx_wakeup_requested()) {
        pause_all_vcpus();
        cpu_synchronize_all_states();
        vmx_system_reset(VMRESET_SILENT);
        veertu_notifiers_notify(&wakeup_notifiers, &wakeup_reason);
        wakeup_reason = QEMU_WAKEUP_REASON_NONE;
        resume_all_vcpus();
        qapi_event_send_wakeup(&error_abort);
    }
    if (vmx_powerdown_requested()) {
        vmx_system_powerdown();
    }
    if (vmx_vmstop_requested(&r)) {
        vm_stop(r);
    }
    return false;
}

static void vmx_process_events(void)
{
    bool nonblocking;
    int last_io = 0;
#ifdef CONFIG_PROFILER
    int64_t ti;
#endif
    do {
        nonblocking = !vmx_enabled() && last_io > 0;
#ifdef CONFIG_PROFILER
        ti = profile_getclock();
#endif
        last_io = main_loop_wait(nonblocking);
#ifdef CONFIG_PROFILER
        dev_time += profile_getclock() - ti;
#endif
    } while (!main_loop_should_exit());
}

static void help(int exitcode)
{
    printf("usage: %s [options] [disk_image]\n\n"
           "'disk_image' is a raw hard disk image for IDE hard disk 0\n\n",
            error_get_progname());

#define QEMU_OPTIONS_GENERATE_HELP
#include "qemu-options-wrapper.h"

    printf("\nDuring emulation, the following keys are useful:\n"
           "ctrl-alt-f      toggle full screen\n"
           "ctrl-alt-n      switch to virtual console 'n'\n"
           "ctrl-alt        toggle mouse and keyboard grab\n"
           "\n"
           "When using -nographic, press 'ctrl-a h' to get some help.\n");

    exit(exitcode);
}

#define HAS_ARG 0x0001

typedef struct QEMUOption {
    const char *name;
    int flags;
    int index;
    uint32_t arch_mask;
} QEMUOption;

static const QEMUOption vmx_options[] = {
    { "h", 0, QEMU_OPTION_h, QEMU_ARCH_ALL },
#define QEMU_OPTIONS_GENERATE_OPTIONS
#include "qemu-options-wrapper.h"
    { NULL },
};

static bool vga_available(void)
{
    return get_type_class("VGA") || get_type_class("isa-vga");
}

static bool cirrus_vga_available(void)
{
    return get_type_class("cirrus-vga")
           || get_type_class("isa-cirrus-vga");
}

static bool vmware_vga_available(void)
{
    return get_type_class("vmware-svga");
}

static bool qxl_vga_available(void)
{
    return get_type_class("qxl-vga");
}

static bool tcx_vga_available(void)
{
    return get_type_class("SUNW,tcx");
}

static bool cg3_vga_available(void)
{
    return get_type_class("cgthree");
}

static void select_vgahw (const char *p)
{
    const char *opts;

    assert(vga_interface_type == VGA_NONE);
    if (strstart(p, "std", &opts)) {
        if (vga_available()) {
            vga_interface_type = VGA_STD;
        } else {
            fprintf(stderr, "Error: standard VGA not available\n");
            exit(0);
        }
    } else if (strstart(p, "cirrus", &opts)) {
        if (cirrus_vga_available()) {
            vga_interface_type = VGA_CIRRUS;
        } else {
            fprintf(stderr, "Error: Cirrus VGA not available\n");
            exit(0);
        }
    } else if (strstart(p, "vmware", &opts)) {
        if (vmware_vga_available()) {
            vga_interface_type = VGA_VMWARE;
        } else {
            fprintf(stderr, "Error: VMWare SVGA not available\n");
            exit(0);
        }
    } else if (strstart(p, "qxl", &opts)) {
        if (qxl_vga_available()) {
            vga_interface_type = VGA_QXL;
        } else {
            fprintf(stderr, "Error: QXL VGA not available\n");
            exit(0);
        }
    } else if (strstart(p, "tcx", &opts)) {
        if (tcx_vga_available()) {
            vga_interface_type = VGA_TCX;
        } else {
            fprintf(stderr, "Error: TCX framebuffer not available\n");
            exit(0);
        }
    } else if (strstart(p, "cg3", &opts)) {
        if (cg3_vga_available()) {
            vga_interface_type = VGA_CG3;
        } else {
            fprintf(stderr, "Error: CG3 framebuffer not available\n");
            exit(0);
        }
    } else if (!strstart(p, "none", &opts)) {
    invalid_vga:
        fprintf(stderr, "Unknown vga type: %s\n", p);
        exit(1);
    }
    while (*opts) {
        const char *nextopt;

        if (strstart(opts, ",retrace=", &nextopt)) {
            opts = nextopt;
            if (strstart(opts, "dumb", &nextopt))
                vga_retrace_method = VGA_RETRACE_DUMB;
            else if (strstart(opts, "precise", &nextopt))
                vga_retrace_method = VGA_RETRACE_PRECISE;
            else goto invalid_vga;
        } else goto invalid_vga;
        opts = nextopt;
    }
}

static DisplayType select_display(const char *p)
{
    const char *opts;
    DisplayType display = DT_DEFAULT;

    if (strstart(p, "sdl", &opts)) {
#ifdef CONFIG_SDL
        display = DT_SDL;
        while (*opts) {
            const char *nextopt;

            if (strstart(opts, ",frame=", &nextopt)) {
                opts = nextopt;
                if (strstart(opts, "on", &nextopt)) {
                    no_frame = 0;
                } else if (strstart(opts, "off", &nextopt)) {
                    no_frame = 1;
                } else {
                    goto invalid_sdl_args;
                }
            } else if (strstart(opts, ",alt_grab=", &nextopt)) {
                opts = nextopt;
                if (strstart(opts, "on", &nextopt)) {
                    alt_grab = 1;
                } else if (strstart(opts, "off", &nextopt)) {
                    alt_grab = 0;
                } else {
                    goto invalid_sdl_args;
                }
            } else if (strstart(opts, ",ctrl_grab=", &nextopt)) {
                opts = nextopt;
                if (strstart(opts, "on", &nextopt)) {
                    ctrl_grab = 1;
                } else if (strstart(opts, "off", &nextopt)) {
                    ctrl_grab = 0;
                } else {
                    goto invalid_sdl_args;
                }
            } else if (strstart(opts, ",window_close=", &nextopt)) {
                opts = nextopt;
                if (strstart(opts, "on", &nextopt)) {
                    no_quit = 0;
                } else if (strstart(opts, "off", &nextopt)) {
                    no_quit = 1;
                } else {
                    goto invalid_sdl_args;
                }
            } else {
            invalid_sdl_args:
                fprintf(stderr, "Invalid SDL option string: %s\n", p);
                exit(1);
            }
            opts = nextopt;
        }
#else
        fprintf(stderr, "SDL support is disabled\n");
        exit(1);
#endif
    } else if (strstart(p, "vnc", &opts)) {
        fprintf(stderr, "VNC support is disabled\n");
        exit(1);
    } else if (strstart(p, "curses", &opts)) {
#ifdef CONFIG_CURSES
        display = DT_CURSES;
#else
        fprintf(stderr, "Curses support is disabled\n");
        exit(1);
#endif
    } else if (strstart(p, "gtk", &opts)) {
#ifdef CONFIG_GTK
        display = DT_GTK;
        while (*opts) {
            const char *nextopt;

            if (strstart(opts, ",grab_on_hover=", &nextopt)) {
                opts = nextopt;
                if (strstart(opts, "on", &nextopt)) {
                    grab_on_hover = true;
                } else if (strstart(opts, "off", &nextopt)) {
                    grab_on_hover = false;
                } else {
                    goto invalid_gtk_args;
                }
            } else {
            invalid_gtk_args:
                fprintf(stderr, "Invalid GTK option string: %s\n", p);
                exit(1);
            }
            opts = nextopt;
        }
#else
        fprintf(stderr, "GTK support is disabled\n");
        exit(1);
#endif
    } else if (strstart(p, "none", &opts)) {
        display = DT_NONE;
    } else {
        fprintf(stderr, "Unknown display type: %s\n", p);
        exit(1);
    }

    return display;
}

static int balloon_parse(const char *arg)
{
    QemuOpts *opts;

    if (strcmp(arg, "none") == 0) {
        return 0;
    }

    return -1;
}

char *vmx_find_file(int type, const char *name)
{
    int i;
    const char *subdir;
    char *buf;

    /* Try the name as a straight path first */
    if (access(name, R_OK) == 0) {
        return g_strdup(name);
    }

    switch (type) {
    case QEMU_FILE_TYPE_BIOS:
        subdir = "";
        break;
    case QEMU_FILE_TYPE_KEYMAP:
        subdir = "keymaps/";
        break;
    default:
        abort();
    }

    for (i = 0; i < data_dir_idx; i++) {
        buf = g_strdup_printf("%s/%s%s", data_dir[i], subdir, name);
        if (access(buf, R_OK) == 0) {
            return buf;
        }
        g_free(buf);
    }
    return NULL;
}

static int device_help_func(QemuOpts *opts, void *opaque)
{
    return qdev_device_help(opts);
}

static int device_init_func(QemuOpts *opts, void *opaque)
{
    DeviceState *dev;

    dev = qdev_device_add(opts);
    if (!dev)
        return -1;
    return 0;
}

static int chardev_init_func(QemuOpts *opts, void *opaque)
{
    Error *local_err = NULL;

    vmx_chr_new_from_opts(opts, NULL, &local_err);
    if (local_err) {
        error_report("%s", error_get_pretty(local_err));
        error_free(local_err);
        return -1;
    }
    return 0;
}

#ifdef CONFIG_VIRTFS
static int fsdev_init_func(QemuOpts *opts, void *opaque)
{
    int ret;
    ret = vmx_fsdev_add(opts);

    return ret;
}
#endif

static int mon_init_func(QemuOpts *opts, void *opaque)
{
    CharDriverState *chr;
    const char *chardev;
    const char *mode;
    int flags;

    mode = vmx_opt_get(opts, "mode");
    if (mode == NULL) {
        mode = "readline";
    }
    if (strcmp(mode, "readline") == 0) {
        flags = MONITOR_USE_READLINE;
    } else if (strcmp(mode, "control") == 0) {
        flags = MONITOR_USE_CONTROL;
    } else {
        fprintf(stderr, "unknown monitor mode \"%s\"\n", mode);
        exit(1);
    }

    if (vmx_opt_get_bool(opts, "pretty", 0))
        flags |= MONITOR_USE_PRETTY;

    if (vmx_opt_get_bool(opts, "default", 0))
        flags |= MONITOR_IS_DEFAULT;

    chardev = vmx_opt_get(opts, "chardev");
    chr = vmx_chr_find(chardev);
    if (chr == NULL) {
        fprintf(stderr, "chardev \"%s\" not found\n", chardev);
        exit(1);
    }

    vmx_chr_fe_claim_no_fail(chr);
    monitor_init(chr, flags);
    return 0;
}

static void monitor_parse(const char *optarg, const char *mode, bool pretty)
{
    static int monitor_device_index = 0;
    QemuOpts *opts;
    const char *p;
    char label[32];
    int def = 0;

    if (strstart(optarg, "chardev:", &p)) {
        snprintf(label, sizeof(label), "%s", p);
    } else {
        snprintf(label, sizeof(label), "compat_monitor%d",
                 monitor_device_index);
        if (monitor_device_index == 0) {
            def = 1;
        }
        opts = vmx_chr_parse_compat(label, optarg);
        if (!opts) {
            fprintf(stderr, "parse error: %s\n", optarg);
            exit(1);
        }
    }

    opts = vmx_opts_create(vmx_find_opts("mon"), label, 1, NULL);
    if (!opts) {
        fprintf(stderr, "duplicate chardev: %s\n", label);
        exit(1);
    }
    vmx_opt_set(opts, "mode", mode);
    vmx_opt_set(opts, "chardev", label);
    vmx_opt_set_bool(opts, "pretty", pretty);
    if (def)
        vmx_opt_set(opts, "default", "on");
    monitor_device_index++;
}

struct device_config {
    enum {
        DEV_USB,       /* -usbdevice     */
        DEV_BT,        /* -bt            */
        DEV_SERIAL,    /* -serial        */
        DEV_PARALLEL,  /* -parallel      */
        DEV_DEBUGCON,  /* -debugcon */
        DEV_GDB,       /* -gdb, -s */
        DEV_SCLP,      /* s390 sclp */
    } type;
    const char *cmdline;
    Location loc;
    QTAILQ_ENTRY(device_config) next;
};

static QTAILQ_HEAD(, device_config) device_configs =
    QTAILQ_HEAD_INITIALIZER(device_configs);

static void add_device_config(int type, const char *cmdline)
{
    struct device_config *conf;

    conf = g_malloc0(sizeof(*conf));
    conf->type = type;
    conf->cmdline = cmdline;
    loc_save(&conf->loc);
    QTAILQ_INSERT_TAIL(&device_configs, conf, next);
}

static int foreach_device_config(int type, int (*func)(const char *cmdline))
{
    struct device_config *conf;
    int rc;

    QTAILQ_FOREACH(conf, &device_configs, next) {
        if (conf->type != type)
            continue;
        loc_push_restore(&conf->loc);
        rc = func(conf->cmdline);
        loc_pop(&conf->loc);
        if (rc) {
            return rc;
        }
    }
    return 0;
}

static int serial_parse(const char *devname)
{
    static int index = 0;
    char label[32];

    if (strcmp(devname, "none") == 0)
        return 0;
    if (index == MAX_SERIAL_PORTS) {
        fprintf(stderr, "veertu: too many serial ports\n");
        exit(1);
    }
    snprintf(label, sizeof(label), "serial%d", index);
    serial_hds[index] = vmx_chr_new(label, devname, NULL);
    if (!serial_hds[index]) {
        fprintf(stderr, "veertu: could not connect serial device"
                " to character backend '%s'\n", devname);
        return -1;
    }
    index++;
    return 0;
}

static int parallel_parse(const char *devname)
{
    static int index = 0;
    char label[32];

    if (strcmp(devname, "none") == 0)
        return 0;
    if (index == MAX_PARALLEL_PORTS) {
        fprintf(stderr, "veertu: too many parallel ports\n");
        exit(1);
    }
    snprintf(label, sizeof(label), "parallel%d", index);
    parallel_hds[index] = vmx_chr_new(label, devname, NULL);
    if (!parallel_hds[index]) {
        fprintf(stderr, "veertu: could not connect parallel device"
                " to character backend '%s'\n", devname);
        return -1;
    }
    index++;
    return 0;
}

static int debugcon_parse(const char *devname)
{
    QemuOpts *opts;

    if (!vmx_chr_new("debugcon", devname, NULL)) {
        exit(1);
    }
    opts = vmx_opts_create(vmx_find_opts("device"), "debugcon", 1, NULL);
    if (!opts) {
        fprintf(stderr, "veertu: already have a debugcon device\n");
        exit(1);
    }
    vmx_opt_set(opts, "driver", "isa-debugcon");
    vmx_opt_set(opts, "chardev", "debugcon");
    return 0;
}

static gint machine_class_cmp(gconstpointer a, gconstpointer b)
{
    const MachineClass *mc1 = a, *mc2 = b;
    int res;

    if (mc1->family == NULL) {
        if (mc2->family == NULL) {
            /* Compare standalone machine types against each other; they sort
             * in increasing order.
             */
            return strcmp((VeertuTypeClassHold(mc1)->type->name),
                          (VeertuTypeClassHold(mc2)->type->name));
        }

        /* Standalone machine types sort after families. */
        return 1;
    }

    if (mc2->family == NULL) {
        /* Families sort before standalone machine types. */
        return -1;
    }

    /* Families sort between each other alphabetically increasingly. */
    res = strcmp(mc1->family, mc2->family);
    if (res != 0) {
        return res;
    }

    /* Within the same family, machine types sort in decreasing order. */
    return strcmp((VeertuTypeClassHold(mc2)->type->name),
                  (VeertuTypeClassHold(mc1)->type->name));
}

 static MachineClass *machine_parse(const char *name)
{
    return NULL;
}

void vmx_add_exit_notifier(Notifier *notify)
{
    veertu_notifiers_add(&exit_notifiers, notify);
}

void vmx_remove_exit_notifier(Notifier *notify)
{
    veertu_notifiers_remove(notify);
}

static void vmx_run_exit_notifiers(void)
{
    veertu_notifiers_notify(&exit_notifiers, NULL);
}

void vmx_add_machine_init_done_notifier(Notifier *notify)
{
    veertu_notifiers_add(&machine_init_done_notifiers, notify);
}

static void vmx_run_machine_init_done_notifiers(void)
{
    veertu_notifiers_notify(&machine_init_done_notifiers, NULL);
}

static const QEMUOption *lookup_opt(int argc, char **argv,
                                    const char **poptarg, int *poptind)
{
    const QEMUOption *popt;
    int optind = *poptind;
    char *r = argv[optind];
    const char *optarg;

    loc_set_cmdline(argv, optind, 1);
    optind++;
    /* Treat --foo the same as -foo.  */
    if (r[1] == '-')
        r++;
    popt = vmx_options;
    for(;;) {
        if (!popt->name) {
            error_report("invalid option");
            exit(1);
        }
        if (!strcmp(popt->name, r + 1))
            break;
        popt++;
    }
    if (popt->flags & HAS_ARG) {
        if (optind >= argc) {
            error_report("requires an argument");
            exit(1);
        }
        optarg = argv[optind++];
        loc_set_cmdline(argv, optind - 2, 2);
    } else {
        optarg = NULL;
    }

    *poptarg = optarg;
    *poptind = optind;

    return popt;
}

static gpointer malloc_and_trace(gsize n_bytes)
{
    void *ptr = malloc(n_bytes);
    return ptr;
}

static gpointer realloc_and_trace(gpointer mem, gsize n_bytes)
{
    void *ptr = realloc(mem, n_bytes);
    return ptr;
}

static void free_and_trace(gpointer mem)
{
    free(mem);
}

static int machine_set_property(const char *name, const char *value,
                                void *opaque)
{
    VeertuType *obj = VeertuTypeHold(opaque);
    Error *local_err = NULL;
    char *c, *qom_name;

    if (strcmp(name, "type") == 0) {
        return 0;
    }
    
    if (strcmp(name, "usb") == 0) {
        MachineState *machine = (MachineState *)opaque;
        machine->usb = value;
        return 0;
    }
    
    if (strcmp(name, "firmware") == 0) {
        MachineState *machine = (MachineState *)opaque;
        machine->firmware = g_strdup(value);
        return 0;
    }
    
    if (strcmp(name, "accel") == 0)
        return 0;

    printf("name is %s\n", name);
    qom_name = g_strdup(name);
    c = qom_name;
    while (*c++) {
        if (*c == '_') {
            *c = '-';
        }
    }

    g_free(qom_name);

    if (local_err) {
        qerror_report_err(local_err);
        error_free(local_err);
        return -1;
    }

    return 0;
}

static int object_create(QemuOpts *opts, void *opaque)
{
    Error *err = NULL;
    char *type = NULL;
    char *id = NULL;
    void *dummy = NULL;
    OptsVisitor *ov;
    QDict *pdict;

    ov = opts_visitor_new(opts);
    pdict = vmx_opts_to_qdict(opts, NULL);

    visit_start_struct(opts_get_visitor(ov), &dummy, NULL, NULL, 0, &err);
    if (err) {
        goto out;
    }

    qdict_del(pdict, "qom-type");
    visit_type_str(opts_get_visitor(ov), &type, "qom-type", &err);
    if (err) {
        goto out;
    }

    qdict_del(pdict, "id");
    visit_type_str(opts_get_visitor(ov), &id, "id", &err);
    if (err) {
        goto out;
    }

    object_add(type, id, pdict, opts_get_visitor(ov), &err);
    if (err) {
        goto out;
    }
    visit_end_struct(opts_get_visitor(ov), &err);
    if (err) {
        qmp_object_del(id, NULL);
    }

out:
    opts_visitor_cleanup(ov);

    QDECREF(pdict);
    g_free(id);
    g_free(type);
    g_free(dummy);
    if (err) {
        qerror_report_err(err);
        error_free(err);
        return -1;
    }
    return 0;
}

static void set_memory_options(uint64_t *ram_slots, ram_addr_t *maxram_size)
{
    uint64_t sz;
    const char *mem_str;
    const char *maxmem_str, *slots_str;
    const ram_addr_t default_ram_size = (ram_addr_t)DEFAULT_RAM_SIZE *
                                        1024 * 1024;
    QemuOpts *opts = vmx_find_opts_singleton("memory");

    sz = 0;
    mem_str = vmx_opt_get(opts, "size");
    if (mem_str) {
        if (!*mem_str) {
            error_report("missing 'size' option value");
            exit(EXIT_FAILURE);
        }

        sz = vmx_opt_get_size(opts, "size", ram_size);

        /* Fix up legacy suffix-less format */
        if (g_ascii_isdigit(mem_str[strlen(mem_str) - 1])) {
            uint64_t overflow_check = sz;

            sz <<= 20;
            if ((sz >> 20) != overflow_check) {
                error_report("too large 'size' option value");
                exit(EXIT_FAILURE);
            }
        }
    }

    /* backward compatibility behaviour for case "-m 0" */
    if (sz == 0) {
        sz = default_ram_size;
    }

    sz = QEMU_ALIGN_UP(sz, 8192);
    ram_size = sz;
    if (ram_size != sz) {
        error_report("ram size too large");
        exit(EXIT_FAILURE);
    }

    /* store value for the future use */
    vmx_opt_set_number(opts, "size", ram_size);
    *maxram_size = ram_size;

    maxmem_str = vmx_opt_get(opts, "maxmem");
    slots_str = vmx_opt_get(opts, "slots");
    if (maxmem_str && slots_str) {
        uint64_t slots;

        sz = vmx_opt_get_size(opts, "maxmem", 0);
        if (sz < ram_size) {
            error_report("invalid -m option value: maxmem "
                    "(0x%" PRIx64 ") <= initial memory (0x"
                    RAM_ADDR_FMT ")", sz, ram_size);
            exit(EXIT_FAILURE);
        }

        slots = vmx_opt_get_number(opts, "slots", 0);
        if ((sz > ram_size) && !slots) {
            error_report("invalid -m option value: maxmem "
                    "(0x%" PRIx64 ") more than initial memory (0x"
                    RAM_ADDR_FMT ") but no hotplug slots where "
                    "specified", sz, ram_size);
            exit(EXIT_FAILURE);
        }

        if ((sz <= ram_size) && slots) {
            error_report("invalid -m option value:  %"
                    PRIu64 " hotplug slots where specified but "
                    "maxmem (0x%" PRIx64 ") <= initial memory (0x"
                    RAM_ADDR_FMT ")", slots, sz, ram_size);
            exit(EXIT_FAILURE);
        }
        *maxram_size = sz;
        *ram_slots = slots;
    } else if ((!maxmem_str && slots_str) ||
            (maxmem_str && !slots_str)) {
        error_report("invalid -m option value: missing "
                "'%s' option", slots_str ? "maxmem" : "slots");
        exit(EXIT_FAILURE);
    }
}

#define MAX_PORT_FWD 8

int veertu_run(int argc, char **argv, char **envp)
{
    int i;
    int snapshot, linux_boot;
    const char *initrd_filename;
    const char *kernel_filename, *kernel_cmdline;
    const char *boot_order;
    DisplayState *ds;
    int cyls, heads, secs, translation;
    QemuOpts *hda_opts = NULL, *opts, *machine_opts, *icount_opts = NULL;
    QemuOptsList *olist;
    int optind;
    const char *optarg;
    const char *loadvm = NULL;
    bool delete_snapshot = false;
    MachineClass *machine_class;
    const char *cpu_model;
    const char *vga_model = NULL;
    const char *qtest_chrdev = NULL;
    const char *qtest_log = NULL;
    const char *pid_file = NULL;
    const char *incoming = NULL;
    bool defconfig = true;
    bool userconfig = true;
    const char *log_mask = NULL;
    const char *log_file = NULL;
    GMemVTable mem_trace = {
        .malloc = malloc_and_trace,
        .realloc = realloc_and_trace,
        .free = free_and_trace,
    };
    const char *trace_events = NULL;
    const char *trace_file = NULL;
    ram_addr_t maxram_size;
    uint64_t ram_slots = 0;
    FILE *vmstate_dump_file = NULL;
    Error *main_loop_err = NULL;
    const char *port_fwd[MAX_PORT_FWD] = {NULL};
    int num_port_fwd = 0;

    atexit(vmx_run_exit_notifiers);
    error_set_progname(argv[0]);
    vmx_init_exec_dir(argv[0]);

    g_mem_set_vtable(&mem_trace);

    veertu_moudle_call_init(3);

    vmx_add_opts(&vmx_drive_opts);
    vmx_add_drive_opts(&vmx_legacy_drive_opts);
    vmx_add_drive_opts(&vmx_common_drive_opts);
    vmx_add_drive_opts(&vmx_drive_opts);
    vmx_add_opts(&vmx_chardev_opts);
    vmx_add_opts(&vmx_device_opts);
    vmx_add_opts(&vmx_netdev_opts);
    vmx_add_opts(&vmx_net_opts);
    vmx_add_opts(&vmx_rtc_opts);
    vmx_add_opts(&vmx_global_opts);
    vmx_add_opts(&vmx_mon_opts);
    vmx_add_opts(&vmx_trace_opts);
    vmx_add_opts(&vmx_option_rom_opts);
    vmx_add_opts(&vmx_machine_opts);
    vmx_add_opts(&vmx_mem_opts);
    vmx_add_opts(&vmx_smp_opts);
    vmx_add_opts(&vmx_boot_opts);
    vmx_add_opts(&vmx_sandbox_opts);
    vmx_add_opts(&vmx_add_fd_opts);
    vmx_add_opts(&vmx_object_opts);
    vmx_add_opts(&vmx_tpmdev_opts);
    vmx_add_opts(&vmx_realtime_opts);
    vmx_add_opts(&vmx_msg_opts);
    vmx_add_opts(&vmx_icount_opts);

    runstate_init();

    rtc_clock = QEMU_CLOCK_HOST;

    QLIST_INIT (&vm_change_state_head);
    os_setup_early_signal_handling();

    veertu_moudle_call_init(1);
    machine_class = find_default_machine();
    cpu_model = NULL;
    snapshot = 0;
    cyls = heads = secs = 0;
    translation = BIOS_ATA_TRANSLATION_AUTO;

    for (i = 0; i < MAX_NODES; i++) {
        numa_info[i].node_mem = 0;
        numa_info[i].present = false;
        bitmap_zero(numa_info[i].node_cpu, MAX_CPUMASK_BITS);
    }

    nb_numa_nodes = 0;
    max_numa_nodeid = 0;
    nb_nics = 0;

    bdrv_init_with_whitelist();

    autostart = 1;

    /* first pass of option parsing */
    optind = 1;
    while (optind < argc) {
        if (argv[optind][0] != '-') {
            /* disk image */
            optind++;
        } else {
            const QEMUOption *popt;

            popt = lookup_opt(argc, argv, &optarg, &optind);
            switch (popt->index) {
            case QEMU_OPTION_nodefconfig:
                defconfig = false;
                break;
            case QEMU_OPTION_nouserconfig:
                userconfig = false;
                break;
            }
        }
    }

    if (defconfig) {
        int ret;
        ret = vmx_read_default_config_files(userconfig);
        if (ret < 0) {
            exit(1);
        }
    }

    /* second pass of option parsing */
    optind = 1;
    for(;;) {
        if (optind >= argc)
            break;
        if (argv[optind][0] != '-') {
            hda_opts = drive_add(IF_DEFAULT, 0, argv[optind++], HD_OPTS);
        } else {
            const QEMUOption *popt;

            popt = lookup_opt(argc, argv, &optarg, &optind);
            if (!(popt->arch_mask & arch_type)) {
                printf("Option %s not supported for this target\n", popt->name);
                exit(1);
            }
            switch(popt->index) {
            case QEMU_OPTION_cpu:
                /* hw initialization will check this */
                cpu_model = optarg;
                break;
            case QEMU_OPTION_hda:
                {
                    char buf[256];
                    if (cyls == 0)
                        snprintf(buf, sizeof(buf), "%s", HD_OPTS);
                    else
                        snprintf(buf, sizeof(buf),
                                 "%s,cyls=%d,heads=%d,secs=%d%s",
                                 HD_OPTS , cyls, heads, secs,
                                 translation == BIOS_ATA_TRANSLATION_LBA ?
                                 ",trans=lba" :
                                 translation == BIOS_ATA_TRANSLATION_NONE ?
                                 ",trans=none" : "");
                    drive_add(IF_DEFAULT, 0, optarg, buf);
                    break;
                }
            case QEMU_OPTION_hdb:
            case QEMU_OPTION_hdc:
            case QEMU_OPTION_hdd:
                drive_add(IF_DEFAULT, popt->index - QEMU_OPTION_hda, optarg,
                          HD_OPTS);
                break;
            case QEMU_OPTION_drive:
                if (drive_def(optarg) == NULL) {
                    exit(1);
                }
                break;
            case QEMU_OPTION_set:
                if (vmx_set_option(optarg) != 0)
                    exit(1);
                break;
            case QEMU_OPTION_global:
                if (vmx_global_option(optarg) != 0)
                    exit(1);
                break;
            case QEMU_OPTION_mtdblock:
                drive_add(IF_MTD, -1, optarg, MTD_OPTS);
                break;
            case QEMU_OPTION_sd:
                drive_add(IF_SD, -1, optarg, SD_OPTS);
                break;
            case QEMU_OPTION_pflash:
                drive_add(IF_PFLASH, -1, optarg, PFLASH_OPTS);
                break;
            case QEMU_OPTION_snapshot:
                snapshot = 1;
                break;
            case QEMU_OPTION_hdachs:
                {
                    const char *p;
                    p = optarg;
                    cyls = strtol(p, (char **)&p, 0);
                    if (cyls < 1 || cyls > 16383)
                        goto chs_fail;
                    if (*p != ',')
                        goto chs_fail;
                    p++;
                    heads = strtol(p, (char **)&p, 0);
                    if (heads < 1 || heads > 16)
                        goto chs_fail;
                    if (*p != ',')
                        goto chs_fail;
                    p++;
                    secs = strtol(p, (char **)&p, 0);
                    if (secs < 1 || secs > 63)
                        goto chs_fail;
                    if (*p == ',') {
                        p++;
                        if (!strcmp(p, "large")) {
                            translation = BIOS_ATA_TRANSLATION_LARGE;
                        } else if (!strcmp(p, "rechs")) {
                            translation = BIOS_ATA_TRANSLATION_RECHS;
                        } else if (!strcmp(p, "none")) {
                            translation = BIOS_ATA_TRANSLATION_NONE;
                        } else if (!strcmp(p, "lba")) {
                            translation = BIOS_ATA_TRANSLATION_LBA;
                        } else if (!strcmp(p, "auto")) {
                            translation = BIOS_ATA_TRANSLATION_AUTO;
                        } else {
                            goto chs_fail;
                        }
                    } else if (*p != '\0') {
                    chs_fail:
                        fprintf(stderr, "veertu: invalid physical CHS format\n");
                        exit(1);
                    }
                    if (hda_opts != NULL) {
                        char num[16];
                        snprintf(num, sizeof(num), "%d", cyls);
                        vmx_opt_set(hda_opts, "cyls", num);
                        snprintf(num, sizeof(num), "%d", heads);
                        vmx_opt_set(hda_opts, "heads", num);
                        snprintf(num, sizeof(num), "%d", secs);
                        vmx_opt_set(hda_opts, "secs", num);
                        if (translation == BIOS_ATA_TRANSLATION_LARGE) {
                            vmx_opt_set(hda_opts, "trans", "large");
                        } else if (translation == BIOS_ATA_TRANSLATION_RECHS) {
                            vmx_opt_set(hda_opts, "trans", "rechs");
                        } else if (translation == BIOS_ATA_TRANSLATION_LBA) {
                            vmx_opt_set(hda_opts, "trans", "lba");
                        } else if (translation == BIOS_ATA_TRANSLATION_NONE) {
                            vmx_opt_set(hda_opts, "trans", "none");
                        }
                    }
                }
                break;
            case QEMU_OPTION_numa:
                opts = vmx_opts_parse(vmx_find_opts("numa"), optarg, 1);
                if (!opts) {
                    exit(1);
                }
                break;
            case QEMU_OPTION_display:
                display_type = select_display(optarg);
                break;
            case QEMU_OPTION_nographic:
                display_type = DT_NOGRAPHIC;
                break;
            case QEMU_OPTION_kernel:
                vmx_opts_set(vmx_find_opts("machine"), 0, "kernel", optarg);
                break;
            case QEMU_OPTION_initrd:
                vmx_opts_set(vmx_find_opts("machine"), 0, "initrd", optarg);
                break;
            case QEMU_OPTION_append:
                vmx_opts_set(vmx_find_opts("machine"), 0, "append", optarg);
                break;
            case QEMU_OPTION_dtb:
                vmx_opts_set(vmx_find_opts("machine"), 0, "dtb", optarg);
                break;
            case QEMU_OPTION_cdrom:
                drive_add(IF_DEFAULT, 2, optarg, CDROM_OPTS);
                break;
            case QEMU_OPTION_boot:
                opts = vmx_opts_parse(vmx_find_opts("boot-opts"), optarg, 1);
                if (!opts) {
                    exit(1);
                }
                break;
            case QEMU_OPTION_fda:
            case QEMU_OPTION_fdb:
                drive_add(IF_FLOPPY, popt->index - QEMU_OPTION_fda,
                          optarg, FD_OPTS);
                break;
            case QEMU_OPTION_no_fd_bootchk:
                fd_bootchk = 0;
                break;
            case QEMU_OPTION_netdev:
                if (net_client_parse(vmx_find_opts("netdev"), optarg) == -1) {
                    exit(1);
                }
                break;
            case QEMU_OPTION_net:
                if (net_client_parse(vmx_find_opts("net"), optarg) == -1) {
                    exit(1);
                }
                break;
#ifdef CONFIG_LIBISCSI
            case QEMU_OPTION_iscsi:
                opts = vmx_opts_parse(vmx_find_opts("iscsi"), optarg, 0);
                if (!opts) {
                    exit(1);
                }
                break;
#endif
#ifdef CONFIG_SLIRP
            case QEMU_OPTION_tftp:
                legacy_tftp_prefix = optarg;
                break;
            case QEMU_OPTION_bootp:
                legacy_bootp_filename = optarg;
                break;
            case QEMU_OPTION_redir:
                if (net_slirp_redir(optarg) < 0)
                    exit(1);
                break;
#endif
            case QEMU_OPTION_h:
                help(0);
                break;
            case QEMU_OPTION_version:
                    printf("Veertu 1.0, Copyright 2015 Veertu Ltd.\n");
                exit(0);
                break;
            case QEMU_OPTION_m:
                opts = vmx_opts_parse(vmx_find_opts("memory"),
                                       optarg, 1);
                if (!opts) {
                    exit(EXIT_FAILURE);
                }
                break;
#ifdef CONFIG_TPM
            case QEMU_OPTION_tpmdev:
                if (tpm_config_parse(vmx_find_opts("tpmdev"), optarg) < 0) {
                    exit(1);
                }
                break;
#endif
            case QEMU_OPTION_mempath:
                mem_path = optarg;
                break;
            case QEMU_OPTION_mem_prealloc:
                mem_prealloc = 1;
                break;
            case QEMU_OPTION_d:
                log_mask = optarg;
                break;
            case QEMU_OPTION_D:
                log_file = optarg;
                break;
            case QEMU_OPTION_s:
                //add_device_config(DEV_GDB, "tcp::" DEFAULT_GDBSTUB_PORT);
                break;
            case QEMU_OPTION_gdb:
                add_device_config(DEV_GDB, optarg);
                break;
            case QEMU_OPTION_L:
                if (data_dir_idx < ARRAY_SIZE(data_dir)) {
                    data_dir[data_dir_idx++] = optarg;
                }
                break;
            case QEMU_OPTION_bios:
                break;
            case QEMU_OPTION_singlestep:
                singlestep = 1;
                break;
            case QEMU_OPTION_S:
                autostart = 0;
                break;
            case QEMU_OPTION_k:
                keyboard_layout = optarg;
                break;
            case QEMU_OPTION_localtime:
                rtc_utc = 0;
                break;
            case QEMU_OPTION_vga:
                vga_model = optarg;
                default_vga = 0;
                break;
            case QEMU_OPTION_monitor:
                default_monitor = 0;
                if (strncmp(optarg, "none", 4)) {
                    monitor_parse(optarg, "readline", false);
                }
                break;
            case QEMU_OPTION_qmp:
                monitor_parse(optarg, "control", false);
                default_monitor = 0;
                break;
            case QEMU_OPTION_qmp_pretty:
                monitor_parse(optarg, "control", true);
                default_monitor = 0;
                break;
            case QEMU_OPTION_mon:
                opts = vmx_opts_parse(vmx_find_opts("mon"), optarg, 1);
                if (!opts) {
                    exit(1);
                }
                default_monitor = 0;
                break;
            case QEMU_OPTION_chardev:
                opts = vmx_opts_parse(vmx_find_opts("chardev"), optarg, 1);
                if (!opts) {
                    exit(1);
                }
                break;
            case QEMU_OPTION_serial:
                add_device_config(DEV_SERIAL, optarg);
                default_serial = 0;
                if (strncmp(optarg, "mon:", 4) == 0) {
                    default_monitor = 0;
                }
                break;
            case QEMU_OPTION_parallel:
                add_device_config(DEV_PARALLEL, optarg);
                default_parallel = 0;
                if (strncmp(optarg, "mon:", 4) == 0) {
                    default_monitor = 0;
                }
                break;
            case QEMU_OPTION_debugcon:
                add_device_config(DEV_DEBUGCON, optarg);
                break;
            case QEMU_OPTION_loadvm:
                loadvm = optarg;
                break;
            case QEMU_OPTION_loadvm2:
                loadvm = optarg;
                delete_snapshot = true;
                break;
            case QEMU_OPTION_pidfile:
                pid_file = optarg;
                break;
            case QEMU_OPTION_acpitable:
                opts = vmx_opts_parse(vmx_find_opts("acpi"), optarg, 1);
                if (!opts) {
                    exit(1);
                }
                do_acpitable_option(opts);
                break;
            case QEMU_OPTION_smbios:
                opts = vmx_opts_parse(vmx_find_opts("smbios"), optarg, 0);
                if (!opts) {
                    exit(1);
                }
                do_smbios_option(opts);
                break;
            case QEMU_OPTION_enable_vmx:
                olist = vmx_find_opts("machine");
                vmx_opts_parse(olist, "accel=vmx", 0);
                break;
            case QEMU_OPTION_M:
            case QEMU_OPTION_machine:
                olist = vmx_find_opts("machine");
                opts = vmx_opts_parse(olist, optarg, 1);
                if (!opts) {
                    exit(1);
                }
                break;
            case QEMU_OPTION_usb:
                olist = vmx_find_opts("machine");
                vmx_opts_parse(olist, "usb=on", 0);
                break;
            case QEMU_OPTION_usbdevice:
                olist = vmx_find_opts("machine");
                vmx_opts_parse(olist, "usb=on", 0);
                add_device_config(DEV_USB, optarg);
                break;
            case QEMU_OPTION_device:
                if (!vmx_opts_parse(vmx_find_opts("device"), optarg, 1)) {
                    exit(1);
                }
                break;
            case QEMU_OPTION_smp:
                if (!vmx_opts_parse(vmx_find_opts("smp-opts"), optarg, 1)) {
                    exit(1);
                }
                break;
            case QEMU_OPTION_vnc:
                fprintf(stderr, "VNC support is disabled\n");
                exit(1);
                break;
            case QEMU_OPTION_no_acpi:
                acpi_enabled = 0;
                break;
            case QEMU_OPTION_no_hpet:
                no_hpet = 1;
                break;
            case QEMU_OPTION_no_hyperv:
                no_hyperv = 1;
                break;
            case QEMU_OPTION_no_reboot:
                no_reboot = 1;
                break;
            case QEMU_OPTION_no_shutdown:
                no_shutdown = 1;
                break;
            case QEMU_OPTION_option_rom:
                if (nb_option_roms >= MAX_OPTION_ROMS) {
                    fprintf(stderr, "Too many option ROMs\n");
                    exit(1);
                }
                opts = vmx_opts_parse(vmx_find_opts("option-rom"), optarg, 1);
                if (!opts) {
                    exit(1);
                }
                option_rom[nb_option_roms].name = vmx_opt_get(opts, "romfile");
                option_rom[nb_option_roms].bootindex =
                    vmx_opt_get_number(opts, "bootindex", -1);
                if (!option_rom[nb_option_roms].name) {
                    fprintf(stderr, "Option ROM file is not specified\n");
                    exit(1);
                }
                nb_option_roms++;
                break;
            case QEMU_OPTION_clock:
                /* Clock options no longer exist.  Keep this option for
                 * backward compatibility.
                 */
                break;
            case QEMU_OPTION_startdate:
                configure_rtc_date_offset(optarg, 1);
                break;
            case QEMU_OPTION_rtc:
                opts = vmx_opts_parse(vmx_find_opts("rtc"), optarg, 0);
                if (!opts) {
                    exit(1);
                }
                configure_rtc(opts);
                break;
            case QEMU_OPTION_nodefaults:
                has_defaults = 0;
                break;
            case QEMU_OPTION_trace:
            {
                opts = vmx_opts_parse(vmx_find_opts("trace"), optarg, 0);
                if (!opts) {
                    exit(1);
                }
                trace_events = vmx_opt_get(opts, "events");
                trace_file = vmx_opt_get(opts, "file");
                break;
            }
            case QEMU_OPTION_add_fd:
#ifndef _WIN32
                opts = vmx_opts_parse(vmx_find_opts("add-fd"), optarg, 0);
                if (!opts) {
                    exit(1);
                }
#else
                error_report("File descriptor passing is disabled on this "
                             "platform");
                exit(1);
#endif
                break;
            case QEMU_OPTION_object:
                opts = vmx_opts_parse(vmx_find_opts("object"), optarg, 1);
                if (!opts) {
                    exit(1);
                }
                break;
            case QEMU_OPTION_realtime:
                opts = vmx_opts_parse(vmx_find_opts("realtime"), optarg, 0);
                if (!opts) {
                    exit(1);
                }
                enable_mlock = vmx_opt_get_bool(opts, "mlock", true);
                break;
            case QEMU_OPTION_msg:
                opts = vmx_opts_parse(vmx_find_opts("msg"), optarg, 0);
                if (!opts) {
                    exit(1);
                }
                configure_msg(opts);
                break;
            case QEMU_OPTION_dump_vmstate:
                if (vmstate_dump_file) {
                    fprintf(stderr, "veertu: only one '-dump-vmstate' "
                            "option may be given\n");
                    exit(1);
                }
                vmstate_dump_file = fopen(optarg, "w");
                if (vmstate_dump_file == NULL) {
                    fprintf(stderr, "open %s: %s\n", optarg, strerror(errno));
                    exit(1);
                }
                break;
            case QEMU_OPTION_expose_hyperv:
                break;
            case QEMU_OPTION_port_fwd:
                if (num_port_fwd < MAX_PORT_FWD)
                    port_fwd[num_port_fwd++] = optarg;
                break;
            default:
                os_parse_cmd_args(popt->index, optarg);
            }
        }
    }

    opts = vmx_get_machine_opts();
    optarg = vmx_opt_get(opts, "type");
    if (optarg) {
        machine_class = machine_parse(optarg);
    }

    set_memory_options(&ram_slots, &maxram_size);

    loc_set_none();

    os_daemonize();

    if (vmx_process_events_init(&main_loop_err)) {
        error_report("%s", error_get_pretty(main_loop_err));
        exit(1);
    }

    if (vmx_opts_foreach(vmx_find_opts("sandbox"), parse_sandbox, NULL, 0)) {
        exit(1);
    }

#ifndef _WIN32
    if (vmx_opts_foreach(vmx_find_opts("add-fd"), parse_add_fd, NULL, 1)) {
        exit(1);
    }

    if (vmx_opts_foreach(vmx_find_opts("add-fd"), cleanup_add_fd, NULL, 1)) {
        exit(1);
    }
#endif

    if (machine_class == NULL) {
        fprintf(stderr, "No machine specified, and there is no default.\n"
                "Use -machine help to list supported machines!\n");
        exit(1);
    }

    current_machine = MACHINE(vtype_new((
                          VeertuTypeClassHold(machine_class))->type->name));
    if (machine_help_func(vmx_get_machine_opts(), current_machine)) {
        exit(0);
    }

    cpu_exec_init_all();

    if (machine_class->hw_version) {
        vmx_set_version(machine_class->hw_version);
    }

    /* Init GETCPU def lists, based on config
     * - Must be called after all the vmx_read_config_file() calls
     * - Must be called before list_cpus()
     * - Must be called before machine->init()
     */
    cpudef_init();

    if (cpu_model && is_help_option(cpu_model)) {
        list_cpus(stdout, &fprintf, cpu_model);
        exit(0);
    }

    /* Open the logfile at this point, if necessary. We can't open the logfile
     * when encountering either of the logging options (-d or -D) because the
     * other one may be encountered later on the command line, changing the
     * location or level of logging.
     */
    if (log_mask) {
        int mask;
        if (log_file) {
            vmx_set_log_filename(log_file);
        }

        mask = vmx_str_to_log_mask(log_mask);
        if (!mask) {
            vmx_print_log_usage(stdout);
            exit(1);
        }
        vmx_set_log(mask);
    }

    /* If no data_dir is specified then try to find it relative to the
       executable path.  */
    if (data_dir_idx < ARRAY_SIZE(data_dir)) {
        data_dir[data_dir_idx] = os_find_datadir();
        if (data_dir[data_dir_idx] != NULL) {
            data_dir_idx++;
        }
    }
    /* If all else fails use the install path specified when building. */
    if (data_dir_idx < ARRAY_SIZE(data_dir)) {
        data_dir[data_dir_idx++] = CONFIG_QEMU_DATADIR;
    }

    smp_parse(vmx_opts_find(vmx_find_opts("smp-opts"), NULL));

    machine_class->max_cpus = machine_class->max_cpus ?: 1; /* Default to UP */
    /*if (max_cpus > machine_class->max_cpus) {
        fprintf(stderr, "Number of SMP cpus requested (%d), exceeds max cpus "
                "supported by machine `%s' (%d)\n", max_cpus,
                machine_class->name, machine_class->max_cpus);
        exit(1);
    }*/

    /*
     * Get the default machine options from the machine if it is not already
     * specified either by the configuration file or by the command line.
     */
    if (machine_class->default_machine_opts) {
        vmx_opts_set_defaults(vmx_find_opts("machine"),
                               machine_class->default_machine_opts, 0);
    }

    vmx_opts_foreach(vmx_find_opts("device"), default_driver_check, NULL, 0);
    vmx_opts_foreach(vmx_find_opts("global"), default_driver_check, NULL, 0);

    if (!vga_model && !default_vga) {
        vga_interface_type = VGA_DEVICE;
    }
    if (!has_defaults || machine_class->no_serial) {
        default_serial = 0;
    }
    if (!has_defaults || machine_class->no_parallel) {
        default_parallel = 0;
    }
    if (!has_defaults || !machine_class->use_virtcon) {
        default_virtcon = 0;
    }
    if (!has_defaults || !machine_class->use_sclp) {
        default_sclp = 0;
    }
    if (!has_defaults || machine_class->no_floppy) {
        default_floppy = 0;
    }
    if (!has_defaults || machine_class->no_cdrom) {
        default_cdrom = 0;
    }
    if (!has_defaults || machine_class->no_sdcard) {
        default_sdcard = 0;
    }
    if (!has_defaults) {
        default_monitor = 0;
        default_net = 0;
        default_vga = 0;
    }

    if (is_daemonized()) {
        /* According to documentation and historically, -nographic redirects
         * serial port, parallel port and monitor to stdio, which does not work
         * with -daemonize.  We can redirect these to null instead, but since
         * -nographic is legacy, let's just error out.
         * We disallow -nographic only if all other ports are not redirected
         * explicitly, to not break existing legacy setups which uses
         * -nographic _and_ redirects all ports explicitly - this is valid
         * usage, -nographic is just a no-op in this case.
         */
        if (display_type == DT_NOGRAPHIC
            && (default_parallel || default_serial
                || default_monitor || default_virtcon)) {
            fprintf(stderr, "-nographic can not be used with -daemonize\n");
            exit(1);
        }
#ifdef CONFIG_CURSES
        if (display_type == DT_CURSES) {
            fprintf(stderr, "curses display can not be used with -daemonize\n");
            exit(1);
        }
#endif
    }

    if (display_type == DT_NOGRAPHIC) {
        if (default_parallel)
            add_device_config(DEV_PARALLEL, "null");
	/* We do not need backwards compatiblity, hence
	 * always start monitor, even with -nographic
	 * The monitor is essential for the UI to function.
	 */
        if (!default_serial || !default_monitor) {
            if (default_serial)
                add_device_config(DEV_SERIAL, "stdio");
            if (default_sclp) {
                add_device_config(DEV_SCLP, "stdio");
            }
        }
    } else {
        if (default_serial)
            add_device_config(DEV_SERIAL, "vc:80Cx24C");
        if (default_parallel)
            add_device_config(DEV_PARALLEL, "vc:80Cx24C");
        if (default_monitor)
            monitor_parse("vc:80Cx24C", "readline", false);
        if (default_sclp) {
            add_device_config(DEV_SCLP, "vc:80Cx24C");
        }
    }

    if (display_type == DT_DEFAULT && !display_remote) {
#if defined(CONFIG_GTK)
        display_type = DT_GTK;
#elif defined(CONFIG_SDL) || defined(CONFIG_COCOA)
        display_type = DT_SDL;
#endif
    }

    if ((no_frame || alt_grab || ctrl_grab) && display_type != DT_SDL) {
        fprintf(stderr, "-no-frame, -alt-grab and -ctrl-grab are only valid "
                        "for SDL, ignoring option\n");
    }
    if (no_quit && (display_type != DT_GTK && display_type != DT_SDL)) {
        fprintf(stderr, "-no-quit is only valid for GTK and SDL, "
                        "ignoring option\n");
    }

#if defined(CONFIG_GTK)
    if (display_type == DT_GTK) {
        early_gtk_display_init();
    }
#endif

    socket_init();

    if (vmx_opts_foreach(vmx_find_opts("chardev"), chardev_init_func, NULL, 1) != 0)
        exit(1);
#ifdef CONFIG_VIRTFS
    if (vmx_opts_foreach(vmx_find_opts("fsdev"), fsdev_init_func, NULL, 1) != 0) {
        exit(1);
    }
#endif

    if (pid_file && vmx_create_pidfile(pid_file) != 0) {
        fprintf(stderr, "Could not acquire pid file: %s\n", strerror(errno));
        exit(1);
    }

    if (vmx_opts_foreach(vmx_find_opts("device"), device_help_func, NULL, 0)
        != 0) {
        exit(0);
    }

    if (vmx_opts_foreach(vmx_find_opts("object"),
                          object_create, NULL, 0) != 0) {
        exit(1);
    }

    machine_opts = vmx_get_machine_opts();
    if (vmx_opt_foreach(machine_opts, machine_set_property, current_machine,
                         1) < 0) {
        exit(1);
    }

    configure_accelerator(current_machine);

    machine_opts = vmx_get_machine_opts();
    kernel_filename = vmx_opt_get(machine_opts, "kernel");
    initrd_filename = vmx_opt_get(machine_opts, "initrd");
    kernel_cmdline = vmx_opt_get(machine_opts, "append");

    boot_order = machine_class->default_boot_order;
    opts = vmx_opts_find(vmx_find_opts("boot-opts"), NULL);
    if (opts) {
        char *normal_boot_order;
        const char *order, *once;
        Error *local_err = NULL;

        order = vmx_opt_get(opts, "order");
        if (order) {
            validate_bootdevices(order, &local_err);
            if (local_err) {
                error_report("%s", error_get_pretty(local_err));
                exit(1);
            }
            boot_order = order;
        }

        once = vmx_opt_get(opts, "once");
        if (once) {
            validate_bootdevices(once, &local_err);
            if (local_err) {
                error_report("%s", error_get_pretty(local_err));
                exit(1);
            }
            normal_boot_order = g_strdup(boot_order);
            boot_order = once;
            vmx_register_reset(restore_boot_order, normal_boot_order);
        }

        boot_menu = vmx_opt_get_bool(opts, "menu", boot_menu);
        boot_strict = vmx_opt_get_bool(opts, "strict", false);
    }

    if (!kernel_cmdline) {
        kernel_cmdline = "";
        current_machine->kernel_cmdline = (char *)kernel_cmdline;
    }

    linux_boot = (kernel_filename != NULL);

    if (!linux_boot && *kernel_cmdline != '\0') {
        fprintf(stderr, "-append only allowed with -kernel option\n");
        exit(1);
    }

    if (!linux_boot && initrd_filename != NULL) {
        fprintf(stderr, "-initrd only allowed with -kernel option\n");
        exit(1);
    }

    if (!linux_boot && vmx_opt_get(machine_opts, "dtb")) {
        fprintf(stderr, "-dtb only allowed with -kernel option\n");
        exit(1);
    }

    os_set_line_buffering();

    vmx_init_cpu_loop();
    vmx_mutex_lock_iothread();

#ifdef CONFIG_SPICE
    /* spice needs the timers to be initialized by this point */
    vmx_spice_init();
#endif

    cpu_ticks_init();

    /* clean up network at qemu process termination */
    atexit(&net_cleanup);

    if (net_init_clients() < 0) {
        exit(1);
    }

    while (num_port_fwd) {
        if (slirp_used)
            net_slirp_redir(port_fwd[num_port_fwd - 1]);
        else
            vnet_add_port_fwd(port_fwd[num_port_fwd - 1]);
        num_port_fwd--;
    }

#ifdef CONFIG_TPM
    if (tpm_init() < 0) {
        exit(1);
    }
#endif

    /* On 32-bit hosts, QEMU is limited by virtual address space */
    if (ram_size > (2047 << 20) && HOST_LONG_BITS == 32) {
        fprintf(stderr, "veertu: at most 2047 MB RAM can be simulated\n");
        exit(1);
    }

    ram_mig_init();

    /* If the currently selected machine wishes to override the units-per-bus
     * property of its default HBA interface type, do so now. */
    if (machine_class->units_per_default_bus) {
        override_max_devs(machine_class->block_default_type,
                          machine_class->units_per_default_bus);
    }

    /* open the virtual block devices */
    if (snapshot)
        vmx_opts_foreach(vmx_find_opts("drive"), drive_enable_snapshot, NULL, 0);
    if (vmx_opts_foreach(vmx_find_opts("drive"), drive_init_func,
                          &machine_class->block_default_type, 1) != 0) {
        exit(1);
    }

    default_drive(default_cdrom, snapshot, machine_class->block_default_type, 2,
                  CDROM_OPTS);
    default_drive(default_floppy, snapshot, IF_FLOPPY, 0, FD_OPTS);
    default_drive(default_sdcard, snapshot, IF_SD, 0, SD_OPTS);

    if (vmx_opts_foreach(vmx_find_opts("mon"), mon_init_func, NULL, 1) != 0) {
        exit(1);
    }

    if (foreach_device_config(DEV_SERIAL, serial_parse) < 0)
        exit(1);
    if (foreach_device_config(DEV_PARALLEL, parallel_parse) < 0)
        exit(1);
    if (foreach_device_config(DEV_DEBUGCON, debugcon_parse) < 0)
        exit(1);

    /* If no default VGA is requested, the default is "none".  */
    if (default_vga) {
        if (machine_class->default_display) {
            vga_model = machine_class->default_display;
        } else if (cirrus_vga_available()) {
            vga_model = "cirrus";
        } else if (vga_available()) {
            vga_model = "std";
        }
    }
    if (vga_model) {
        select_vgahw(vga_model);
    }

    if (machine_class->compat_props) {
    }

    qdev_machine_init();

    current_machine->ram_size = ram_size;
    current_machine->maxram_size = maxram_size;
    current_machine->ram_slots = ram_slots;
    current_machine->boot_order = boot_order;
    current_machine->cpu_model = cpu_model;

    machine_class->init(current_machine);

    realtime_init();

    audio_init();

    cpu_synchronize_all_post_init();

    /* init USB devices */
    if (usb_enabled()) {
        if (foreach_device_config(DEV_USB, usb_parse) < 0)
            exit(1);
    }

    /* init generic devices */
    if (vmx_opts_foreach(vmx_find_opts("device"), device_init_func, NULL, 1) != 0)
        exit(1);

    /* Did we create any drives that we failed to create a device for? */
    drive_check_orphaned();

    net_check_clients();

    ds = init_displaystate();

    /* init local displays */
    switch (display_type) {
    case DT_NOGRAPHIC:
        (void)ds;	/* avoid warning if no display is configured */
        break;
#if defined(CONFIG_CURSES)
    case DT_CURSES:
        curses_display_init(ds, full_screen);
        break;
#endif
#if defined(CONFIG_SDL)
    case DT_SDL:
        sdl_display_init(ds, full_screen, no_frame);
        break;
#elif defined(CONFIG_COCOA)
    case DT_SDL:
        cocoa_display_init(ds, full_screen);
        break;
#endif
#if defined(CONFIG_GTK)
    case DT_GTK:
        gtk_display_init(ds, full_screen, grab_on_hover);
        break;
#endif
    default:
        break;
    }

    /* must be after terminal init, SDL library changes signal handlers */
    os_setup_signal_handling();

#ifdef CONFIG_SPICE
    if (using_spice) {
        vmx_spice_display_init();
    }
#endif


    qdev_machine_creation_done();

    if (rom_load_all() != 0) {
        fprintf(stderr, "rom loading failed\n");
        exit(1);
    }

    /* TODO: once all bus devices are qdevified, this should be done
     * when bus is created by qdev.c */
    vmx_register_reset(qbus_reset_all_fn, sysbus_get_default());
    vmx_run_machine_init_done_notifiers();

    /* Done notifiers can load ROMs */
    rom_load_done();

    vmx_system_reset(VMRESET_SILENT);
    if ( 1 || loadvm) {
        if (loadvm && load_vmstate(loadvm) < 0) {
            //autostart = 0;
        }
        
        if (1 || delete_snapshot) {
            QDict *q = qdict_new();
            qdict_put(q, "name", qstring_from_str(/*loadvm*/ "last_state"));
            do_delvm(NULL, q);
            g_free(q);
        }
    }

    if (vmstate_dump_file) {
        /* dump and exit */
        dump_vmstate_json_to_file(vmstate_dump_file);
        return 0;
    }

    if (autostart) {
        vm_start();
    }

    os_setup_post();

    vmx_process_events();
    bdrv_close_all();
    pause_all_vcpus();
    res_free();
#ifdef CONFIG_TPM
    tpm_cleanup();
#endif

    return 0;
}

int veertu_run(int argc, char **argv, char **envp);

int vmx_main(const char* vm_path, int argc, char **argv, char **envp)
{
    int i;
    bool restore = false;

    for (i = 1; i < argc; i++) {
        const char *opt = argv[i];
        
        if (opt[0] == '-') {
            /* Treat --foo the same as -foo.  */
            if (opt[1] == '-') {
                opt++;
            }
            if (!strcmp(opt, "-loadvm2"))
                restore = true;
        }
    }

    char buf[2048];
    strcpy(buf, "vmx");
    strcat(buf, " ");
    get_launch_param_string(vm_path, restore, buf + strlen(buf), sizeof(buf) - 1 - strlen(buf));
    //printf(buf);
    
    g_shell_parse_argv (buf, &argc, &argv, NULL);
    for (i=0; i<argc; i++)
        printf("arg %d: %s\n", i, argv[i]);
    int res = veertu_run(argc, argv, NULL);
    g_strfreev(argv);
    return res;
}

void qmp_object_del(const char *id, Error **errp)
{
}

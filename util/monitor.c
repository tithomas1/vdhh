/*
 * QEMU monitor
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
#include <dirent.h>
#include "hw.h"
#include "monitor/qdev.h"
#include "usb.h"
#include "ipc.h"
#include "pci.h"
#include "loader.h"
#include "net/net.h"
#include "net/slirp.h"
#include "emuchar.h"
#include "sysemu.h"
#include "monitor/monitor.h"
#include "qemu/readline.h"
#include "ui/console.h"
#include "ui/input.h"
#include "emublockdev.h"
#include "audio/audio.h"
#include "qemu/timer.h"
#include "migration.h"
#include "veertuemu.h"
#include "qemu/acl.h"
#include "qapi/qmp/qint.h"
#include "qapi/qmp/qfloat.h"
#include "qapi/qmp/qlist.h"
#include "qapi/qmp/qbool.h"
#include "qapi/qmp/qstring.h"
#include "qapi/qmp/qjson.h"
#include "qapi/qmp/json-streamer.h"
#include "qapi/qmp/json-parser.h"
#include "qemu/osdep.h"
#include "cpu.h"
#ifdef CONFIG_TRACE_SIMPLE
#include "trace/simple.h"
#endif
#include "memory.h"
#include "qmp-commands.h"
#include "qemu/thread.h"
#include "qapi.h"
#include "qapi/qmp-event.h"
#include "qapi-event.h"
#include <arpa/inet.h>

/* for pic/irq_info */
#if defined(TARGET_SPARC)
#include "hw/sparc/sun4m.h"
#endif

extern int slirp_used;

struct Monitor {
    CharDriverState *chr;
    int reset_seen;
    int flags;
    int suspend_cnt;
    bool skip_flush;
    
    QemuMutex out_lock;
    QString *outbuf;
    guint out_watch;
    
    /* Read under either BQL or out_lock, written with BQL+out_lock.  */
    int mux_out;
    
    ReadLineState *rs;
    CPUState *mon_cpu;
    void *password_opaque;
    QError *error;
    QLIST_HEAD(,mon_fd_t) fds;
    QLIST_ENTRY(Monitor) entry;
};

Monitor *cur_mon = NULL;
Monitor *default_mon = NULL;

QemuOptsList vmx_mon_opts = {
    .name = "mon",
    .implied_opt_name = "chardev",
    .head = QTAILQ_HEAD_INITIALIZER(vmx_mon_opts.head),
    .desc = {
        {
            .name = "mode",
            .type = QEMU_OPT_STRING,
        },{
            .name = "chardev",
            .type = QEMU_OPT_STRING,
        },{
            .name = "default",
            .type = QEMU_OPT_BOOL,
        },{
            .name = "pretty",
            .type = QEMU_OPT_BOOL,
        },
        { /* end of list */ }
    },
};

int monitor_cur_is_qmp(void)
{
    return 0;
}

static int monitor_can_read(void *opaque)
{
    return 1;
}

static void monitor_flush_locked(Monitor *mon);

static gboolean monitor_unblocked(GIOChannel *chan, GIOCondition cond,
                                  void *opaque)
{
    Monitor *mon = opaque;
    
    vmx_mutex_lock(&mon->out_lock);
    mon->out_watch = 0;
    monitor_flush_locked(mon);
    vmx_mutex_unlock(&mon->out_lock);
    return false;
}

/* Called with mon->out_lock held.  */
static void monitor_flush_locked(Monitor *mon)
{
    int rc;
    size_t len;
    const char *buf;
    
    if (mon->skip_flush) {
        return;
    }
    
    buf = qstring_get_str(mon->outbuf);
    len = qstring_get_length(mon->outbuf);
    
    if (len && !mon->mux_out) {
        rc = vmx_chr_fe_write(mon->chr, (const uint8_t *) buf, len);
        if ((rc < 0 && errno != EAGAIN) || (rc == len)) {
            /* all flushed or error */
            QDECREF(mon->outbuf);
            mon->outbuf = qstring_new();
            return;
        }
        if (rc > 0) {
            /* partinal write */
            QString *tmp = qstring_from_str(buf + rc);
            QDECREF(mon->outbuf);
            mon->outbuf = tmp;
        }
        if (mon->out_watch == 0) {
            mon->out_watch = vmx_chr_fe_add_watch(mon->chr, G_IO_OUT|G_IO_HUP, monitor_unblocked, mon);
        }
    }
}

void monitor_flush(Monitor *mon)
{
    vmx_mutex_lock(&mon->out_lock);
    monitor_flush_locked(mon);
    vmx_mutex_unlock(&mon->out_lock);
}

/* flush at every end of line */
static void monitor_puts(Monitor *mon, const char *str)
{
    char c;
    
    vmx_mutex_lock(&mon->out_lock);
    for(;;) {
        c = *str++;
        if (c == '\0')
            break;
        if (c == '\n') {
            qstring_append_chr(mon->outbuf, '\r');
        }
        qstring_append_chr(mon->outbuf, c);
        if (c == '\n') {
            monitor_flush_locked(mon);
        }
    }
    vmx_mutex_unlock(&mon->out_lock);
}

static void monitor_control_read(void *opaque, const uint8_t *buf, int size)
{
    Monitor *mon = opaque;
    printf("%s\n", buf);
    abort();
    
    monitor_puts(mon, "OK\n");
}

static void monitor_data_destroy(Monitor *mon);

static void monitor_control_event(void *opaque, int event)
{
    Monitor *mon = opaque;
    
    switch (event) {
        case CHR_EVENT_OPENED:
            break;
        case CHR_EVENT_CLOSED:
            //monitor_data_destroy(mon);
            //g_free(mon);
            break;
    }
}

static void monitor_data_init(Monitor *mon)
{
    memset(mon, 0, sizeof(Monitor));
    vmx_mutex_init(&mon->out_lock);
    mon->outbuf = qstring_new();
}

static void monitor_data_destroy(Monitor *mon)
{
    QDECREF(mon->outbuf);
    vmx_mutex_destroy(&mon->out_lock);
}

static void GCC_FMT_ATTR(2, 3) monitor_readline_printf(void *opaque,
                                                       const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    monitor_vprintf(opaque, fmt, ap);
    va_end(ap);
}

static void monitor_readline_flush(void *opaque)
{
    monitor_flush(opaque);
}

void handle_user_command(Monitor *mon, const char *cmd);

static void monitor_read(void *opaque, const uint8_t *buf, int size)
{
    Monitor *old_mon = cur_mon;
    int i;
    
    cur_mon = opaque;
    
    if (cur_mon->rs) {
        for (i = 0; i < size; i++)
            readline_handle_byte(cur_mon->rs, buf[i]);
    } else {
        if (size == 0 || buf[size - 1] != 0) {
        }
        else
            handle_user_command(cur_mon, (const char *)buf);
    }
    
    cur_mon = old_mon;
}

void monitor_init(CharDriverState *chr, int flags)
{
    Monitor *mon;

    mon = g_malloc(sizeof(*mon));
    monitor_data_init(mon);
    
    mon->chr = chr;
    mon->flags = flags;
    /*vmx_chr_fe_set_echo(chr, true);
    vmx_chr_add_handlers(chr, monitor_can_read, monitor_control_read,
                          monitor_control_event, mon);*/

    mon->rs = readline_init(monitor_readline_printf,
                            monitor_readline_flush,
                            mon,
                            NULL);
    monitor_read_command(mon, 0);
    
    vmx_chr_add_handlers(chr, monitor_can_read, monitor_read,
                          monitor_control_event, mon);
}

int monitor_read_bdrv_key_start(Monitor *mon, BlockDriverState *bs,
                                BlockCompletionFunc *completion_cb,
                                void *opaque)
{
    return 0;
}

int monitor_read_block_device_key(Monitor *mon, const char *device,
                                  BlockCompletionFunc *completion_cb,
                                  void *opaque)
{
    return 0;
}

int monitor_get_fd(Monitor *mon, const char *fdname, Error **errp)
{
    return 0;
}

int monitor_handle_fd_param(Monitor *mon, const char *fdname)
{
    return 0;
}

int monitor_handle_fd_param2(Monitor *mon, const char *fdname, Error **errp)
{
    return 0;
}

void monitor_vprintf(Monitor *mon, const char *fmt, va_list ap)
{
    char *buf;
    
    if (!mon)
        return;
    
    /*if (monitor_ctrl_mode(mon)) {
        return;
    }*/
    
    /*buf = g_strdup_vprintf(fmt, ap);
    monitor_puts(mon, buf);
    g_free(buf);*/
}

GCC_FMT_ATTR(2, 0);
void monitor_printf(Monitor *mon, const char *fmt, ...) GCC_FMT_ATTR(2, 3)
{
    va_list ap;
    va_start(ap, fmt);
    monitor_vprintf(mon, fmt, ap);
    va_end(ap);
}

int monitor_set_cpu(int cpu_index)
{
    return 0;
}

int monitor_get_cpu_index(void);

void monitor_set_error(Monitor *mon, QError *qerror)
{
}

struct cmd_handler {
    const char *cmd;
    void (*handler)(Monitor *mon, int argc, char *argv[]);
};

void cmd_status(Monitor *mon, int argc, char *argv[])
{
    monitor_puts(mon, "OK\n");
}

void cmd_shutoff(Monitor *mon, int argc, char *argv[])
{
    monitor_puts(mon, "OK\n");
    exit(0);
}

void suspend_vm();
void shutdown_vm(bool forced);
void reboot_vm(bool forced);
void reset_vm();

void cmd_shutdown(Monitor *mon, int argc, char *argv[])
{
    monitor_puts(mon, "OK\n");
    shutdown_vm(false);
}

void cmd_reboot(Monitor *mon, int argc, char *argv[])
{
    monitor_puts(mon, "OK\n");
    reboot_vm(false);
}

void cmd_suspend(Monitor *mon, int argc, char *argv[])
{
    monitor_puts(mon, "OK\n");
    suspend_vm();
}

void cmd_reset(Monitor *mon, int argc, char *argv[])
{
    monitor_puts(mon, "OK\n");
    reset_vm();
}

uint32_t vm_ip_address;

void cmd_show_ip_address(Monitor *mon, int argc, char *argv[])
{
    char buf[128];
    struct in_addr in;
    in.s_addr = vm_ip_address;

    snprintf(buf, sizeof(buf), "%s\n", inet_ntoa(in));
    char *inet_ntoa(struct in_addr in);

    monitor_puts(mon, buf);
}

void cmd_add_port_forward(Monitor *mon, int argc, char *argv[])
{
    int res = -1;
    if (argc == 2) {
        if (slirp_used)
            res = net_slirp_redir(argv[1]);
        else
            res = vnet_add_port_fwd(argv[1]);
    }
    monitor_puts(mon, res >=0 ? "OK\n" : "FAIL\n");
}

void cmd_del_port_forward(Monitor *mon, int argc, char *argv[])
{
    int res = -1;
    if (argc == 2) {
        if (slirp_used)
            res = net_slirp_redir_del(argv[1]);
        else
            res = vnet_del_port_fwd(argv[1]);
    }
    monitor_puts(mon, res >=0 ? "OK\n" : "FAIL\n");
}


static struct cmd_handler handlers[] = {
    {"status", cmd_status},
    {"shutoff", cmd_shutoff},
    {"reboot", cmd_reboot},
    {"reset", cmd_reset},
    {"shutdown", cmd_shutdown},
    {"suspend", cmd_suspend},
    {"ip_addr", cmd_show_ip_address},
    {"add_port_forward", cmd_add_port_forward},
    {"del_port_forward", cmd_del_port_forward},
};


void handle_user_command(Monitor *mon, const char *cmd)
{
    char **argv = NULL;
    int argc;

    g_shell_parse_argv(cmd, &argc, &argv, NULL);
    if (!argc || !argv)
        return;

    //printf("command: %s\n", cmd);

    for (int i = 0; i < ARRAY_SIZE(handlers); i++) {
        if (!strcasecmp(handlers[i].cmd, argv[0])) {
            handlers[i].handler(mon, argc, argv);
            break;
        }
    }
    g_strfreev(argv);
}


static void monitor_command_cb(void *opaque, const char *cmdline,
                               void *readline_opaque)
{
    Monitor *mon = opaque;
    
    monitor_suspend(mon);
    handle_user_command(mon, cmdline);
    monitor_resume(mon);
}

int monitor_suspend(Monitor *mon)
{
    if (!mon->rs)
        return -ENOTTY;
    mon->suspend_cnt++;
    return 0;
}

void monitor_resume(Monitor *mon)
{
    if (!mon->rs)
        return;
    if (--mon->suspend_cnt == 0)
        readline_show_prompt(mon->rs);
}

void monitor_read_command(Monitor *mon, int show_prompt)
{
    if (!mon->rs)
        return;
    
    readline_start(mon->rs, "vmx:", 0, monitor_command_cb, NULL);
    //if (show_prompt)
      //  readline_show_prompt(mon->rs);
}

ReadLineState *monitor_get_rs(Monitor *mon)
{
    return 0;
}

int monitor_read_password(Monitor *mon, ReadLineFunc *readline_func,
                          void *opaque)
{
    return 0;
}

int qmp_qom_set(Monitor *mon, const QDict *qdict, QObject **ret)
{
    return 0;
}

int qmp_qom_get(Monitor *mon, const QDict *qdict, QObject **ret)
{
    return 0;
}

int qmp_object_add(Monitor *mon, const QDict *qdict, QObject **ret)
{
    return 0;
}

void object_add(const char *type, const char *id, const QDict *qdict,
                Visitor *v, Error **errp)
{
}

AddfdInfo *monitor_fdset_add_fd(int fd, bool has_fdset_id, int64_t fdset_id,
                                bool has_opaque, const char *opaque,
                                Error **errp)
{
    return 0;
}

int monitor_fdset_get_fd(int64_t fdset_id, int flags)
{
    return 0;
}

int monitor_fdset_dup_fd_add(int64_t fdset_id, int dup_fd)
{
    return 0;
}

int monitor_fdset_dup_fd_find(int dup_fd)
{
    return 0;
}



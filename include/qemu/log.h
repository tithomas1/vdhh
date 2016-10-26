#ifndef QEMU_LOG_H
#define QEMU_LOG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include "qemu/compiler.h"
#include "qom/cpu.h"

/* Private global variables, don't use */
extern FILE *vmx_logfile;
extern int vmx_loglevel;

/* 
 * The new API:
 *
 */

/* Log settings checking macros: */

/* Returns true if vmx_log() will really write somewhere
 */
static inline bool vmx_log_enabled(void)
{
    return vmx_logfile != NULL;
}

#define CPU_LOG_TB_OUT_ASM (1 << 0)
#define CPU_LOG_TB_IN_ASM  (1 << 1)
#define CPU_LOG_TB_OP      (1 << 2)
#define CPU_LOG_TB_OP_OPT  (1 << 3)
#define CPU_LOG_INT        (1 << 4)
#define CPU_LOG_EXEC       (1 << 5)
#define CPU_LOG_PCALL      (1 << 6)
#define CPU_LOG_IOPORT     (1 << 7)
#define CPU_LOG_TB_CPU     (1 << 8)
#define CPU_LOG_RESET      (1 << 9)
#define LOG_UNIMP          (1 << 10)
#define LOG_GUEST_ERROR    (1 << 11)
#define CPU_LOG_MMU        (1 << 12)

/* Returns true if a bit is set in the current loglevel mask
 */
static inline bool vmx_loglevel_mask(int mask)
{
    return (vmx_loglevel & mask) != 0;
}

/* Logging functions: */

/* main logging function
 */
void GCC_FMT_ATTR(1, 2) vmx_log(const char *fmt, ...);

/* vfprintf-like logging function
 */
static inline void GCC_FMT_ATTR(1, 0)
vmx_log_vprintf(const char *fmt, va_list va)
{
    if (vmx_logfile) {
        vfprintf(vmx_logfile, fmt, va);
    }
}

/* log only if a bit is set on the current loglevel mask
 */
void GCC_FMT_ATTR(2, 3) vmx_log_mask(int mask, const char *fmt, ...);


/* Special cases: */

/* cpu_dump_state() logging functions: */
/**
 * log_cpu_state:
 * @cpu: The GETCPU whose state is to be logged.
 * @flags: Flags what to log.
 *
 * Logs the output of cpu_dump_state().
 */
static inline void log_cpu_state(CPUState *cpu, int flags)
{
    //if (vmx_log_enabled()) {
    //    cpu_dump_state(cpu, vmx_logfile, fprintf, flags);
    //}
}

/**
 * log_cpu_state_mask:
 * @mask: Mask when to log.
 * @cpu: The GETCPU whose state is to be logged.
 * @flags: Flags what to log.
 *
 * Logs the output of cpu_dump_state() if loglevel includes @mask.
 */
static inline void log_cpu_state_mask(int mask, CPUState *cpu, int flags)
{
    if (vmx_loglevel & mask) {
        log_cpu_state(cpu, flags);
    }
}

//#ifdef NEED_CPU_H
/* disas() and target_disas() to vmx_logfile: */
static inline void log_target_disas(CPUArchState *env, target_ulong start,
                                    target_ulong len, int flags)
{
}

static inline void log_disas(void *code, unsigned long size)
{
}

#if defined(CONFIG_USER_ONLY)
/* page_dump() output to the log file: */
static inline void log_page_dump(void)
{
    page_dump(vmx_logfile);
}
#endif
//#endif


/* Maintenance: */

/* fflush() the log file */
static inline void vmx_log_flush(void)
{
    fflush(vmx_logfile);
}

/* Close the log file */
static inline void vmx_log_close(void)
{
    if (vmx_logfile) {
        if (vmx_logfile != stderr) {
            fclose(vmx_logfile);
        }
        vmx_logfile = NULL;
    }
}

/* Set up a new log file */
static inline void vmx_log_set_file(FILE *f)
{
    vmx_logfile = f;
}

/* define log items */
typedef struct QEMULogItem {
    int mask;
    const char *name;
    const char *help;
} QEMULogItem;

extern const QEMULogItem vmx_log_items[];

/* This is the function that actually does the work of
 * changing the log level; it should only be accessed via
 * the vmx_set_log() wrapper.
 */
void do_vmx_set_log(int log_flags, bool use_own_buffers);

static inline void vmx_set_log(int log_flags)
{
#ifdef CONFIG_USER_ONLY
    do_vmx_set_log(log_flags, true);
#else
    do_vmx_set_log(log_flags, false);
#endif
}

void vmx_set_log_filename(const char *filename);
int vmx_str_to_log_mask(const char *str);

/* Print a usage message listing all the valid logging categories
 * to the specified FILE*.
 */
void vmx_print_log_usage(FILE *f);

#endif


/* Common header file that is included by all of QEMU.
 *
 * This file is supposed to be included only by .c files. No header file should
 * depend on qemu-common.h, as this would easily lead to circular header
 * dependencies.
 *
 * If a header file uses a definition from qemu-common.h, that definition
 * must be moved to a separate header file, and the header that uses it
 * must include that header.
 */
#ifndef QEMU_COMMON_H
#define QEMU_COMMON_H

#include "qemu/compiler.h"
#include "config-host.h"
#include "qemu/typedefs.h"

#if defined(__arm__) || defined(__sparc__) || defined(__mips__) || defined(__hppa__) || defined(__ia64__)
#define WORDS_ALIGNED
#endif

#define TFR(expr) do { if ((expr) != -1) break; } while (errno == EINTR)

/* we put basic includes here to avoid repeating them in device drivers */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <inttypes.h>
#include <limits.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <assert.h>
#include <signal.h>
#include <glib.h>
#include "qemu/option.h"

#ifdef _WIN32
#include "emuos-win32.h"
#endif

#ifdef CONFIG_POSIX
#include "emuos-posix.h"
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif
#ifndef ENOMEDIUM
#define ENOMEDIUM ENODEV
#endif
#if !defined(ENOTSUP)
#define ENOTSUP 4096
#endif
#if !defined(ECANCELED)
#define ECANCELED 4097
#endif
#if !defined(EMEDIUMTYPE)
#define EMEDIUMTYPE 4098
#endif
#ifndef TIME_MAX
#define TIME_MAX LONG_MAX
#endif

/* HOST_LONG_BITS is the size of a native pointer in bits. */
#if UINTPTR_MAX == UINT32_MAX
# define HOST_LONG_BITS 32
#elif UINTPTR_MAX == UINT64_MAX
# define HOST_LONG_BITS 64
#else
# error Unknown pointer size
#endif

typedef int (*fprintf_function)(FILE *f, const char *fmt, ...)
    GCC_FMT_ATTR(2, 3);

#ifdef _WIN32
#define fsync _commit
#if !defined(lseek)
# define lseek _lseeki64
#endif
int vmx_ftruncate64(int, int64_t);
#if !defined(ftruncate)
# define ftruncate vmx_ftruncate64
#endif

static inline char *realpath(const char *path, char *resolved_path)
{
    _fullpath(resolved_path, path, _MAX_PATH);
    return resolved_path;
}
#endif

void cpu_ticks_init(void);

/* icount */
void configure_icount(QemuOpts *opts, Error **errp);
extern int use_icount;
extern int icount_align_option;
/* drift information for info jit command */
extern int64_t max_delay;
extern int64_t max_advance;
void dump_drift_info(FILE *f, fprintf_function cpu_fprintf);

#include "qemu/osdep.h"
#include "qemu/bswap.h"

/* FIXME: Remove NEED_CPU_H.  */
//#ifdef NEED_CPU_H
#include "util/cpu.h"
//#endif /* !defined(NEED_CPU_H) */

/* main function, renamed */
#if defined(CONFIG_COCOA)
int vmx_main(const char* vm_path, int argc, char **argv, char **envp);
#endif

void vmx_get_timedate(struct tm *tm, int offset);
int vmx_timedate_diff(struct tm *tm);

/* Convenience for block device models */
typedef struct BlockConf {
    BlockBackend *blk;
    uint16_t physical_block_size;
    uint16_t logical_block_size;
    uint16_t min_io_size;
    uint32_t opt_io_size;
    int32_t bootindex;
    uint32_t discard_granularity;
    uint32_t cyls, heads, secs;
} BlockConf;

static inline unsigned int get_physical_block_exp(BlockConf *conf)
{
    unsigned int exp = 0, size;
    
    for (size = conf->physical_block_size;
         size > conf->logical_block_size;
         size >>= 1) {
        exp++;
    }
    
    return exp;
}


/* Configuration helpers */

void blkconf_serial(BlockConf *conf, char **serial);
void blkconf_geometry(BlockConf *conf, int *trans,
                      unsigned cyls_max, unsigned heads_max, unsigned secs_max,
                      Error **errp);

/* Hard disk geometry */

void hd_geometry_guess(BlockBackend *blk,
                       uint32_t *pcyls, uint32_t *pheads, uint32_t *psecs,
                       int *ptrans);
int hd_bios_chs_auto_trans(uint32_t cyls, uint32_t heads, uint32_t secs);

/**
 * is_help_option:
 * @s: string to test
 *
 * Check whether @s is one of the standard strings which indicate
 * that the user is asking for a list of the valid values for a
 * command option like -cpu or -M. The current accepted strings
 * are 'help' and '?'. '?' is deprecated (it is a shell wildcard
 * which makes it annoying to use in a reliable way) but provided
 * for backwards compatibility.
 *
 * Returns: true if @s is a request for a list.
 */
static inline bool is_help_option(const char *s)
{
    return !strcmp(s, "?") || !strcmp(s, "help");
}

/* cutils.c */
void pstrcpy(char *buf, int buf_size, const char *str);
void strpadcpy(char *buf, int buf_size, const char *str, char pad);
char *pstrcat(char *buf, int buf_size, const char *s);
int strstart(const char *str, const char *val, const char **ptr);
int stristart(const char *str, const char *val, const char **ptr);
int vmx_strnlen(const char *s, int max_len);
char *vmx_strsep(char **input, const char *delim);
time_t mktimegm(struct tm *tm);
int vmx_fls(int i);
int vmx_fdatasync(int fd);
int fcntl_setfl(int fd, int flag);
int vmx_parse_fd(const char *param);

int parse_uint(const char *s, unsigned long long *value, char **endptr,
               int base);
int parse_uint_full(const char *s, unsigned long long *value, int base);

/*
 * strtosz() suffixes used to specify the default treatment of an
 * argument passed to strtosz() without an explicit suffix.
 * These should be defined using upper case characters in the range
 * A-Z, as strtosz() will use vmx_toupper() on the given argument
 * prior to comparison.
 */
#define STRTOSZ_DEFSUFFIX_EB	'E'
#define STRTOSZ_DEFSUFFIX_PB	'P'
#define STRTOSZ_DEFSUFFIX_TB	'T'
#define STRTOSZ_DEFSUFFIX_GB	'G'
#define STRTOSZ_DEFSUFFIX_MB	'M'
#define STRTOSZ_DEFSUFFIX_KB	'K'
#define STRTOSZ_DEFSUFFIX_B	'B'
int64_t strtosz(const char *nptr, char **end);
int64_t strtosz_suffix(const char *nptr, char **end, const char default_suffix);
int64_t strtosz_suffix_unit(const char *nptr, char **end,
                            const char default_suffix, int64_t unit);

/* used to print char* safely */
#define STR_OR_NULL(str) ((str) ? (str) : "null")

/* id.c */
bool id_wellformed(const char *id);

/* path.c */
void init_paths(const char *prefix);
const char *path(const char *pathname);

#define vmx_isalnum(c)		isalnum((unsigned char)(c))
#define vmx_isalpha(c)		isalpha((unsigned char)(c))
#define vmx_iscntrl(c)		iscntrl((unsigned char)(c))
#define vmx_isdigit(c)		isdigit((unsigned char)(c))
#define vmx_isgraph(c)		isgraph((unsigned char)(c))
#define vmx_islower(c)		islower((unsigned char)(c))
#define vmx_isprint(c)		isprint((unsigned char)(c))
#define vmx_ispunct(c)		ispunct((unsigned char)(c))
#define vmx_isspace(c)		isspace((unsigned char)(c))
#define vmx_isupper(c)		isupper((unsigned char)(c))
#define vmx_isxdigit(c)	isxdigit((unsigned char)(c))
#define vmx_tolower(c)		tolower((unsigned char)(c))
#define vmx_toupper(c)		toupper((unsigned char)(c))
#define vmx_isascii(c)		isascii((unsigned char)(c))
#define vmx_toascii(c)		toascii((unsigned char)(c))

void *vmx_oom_check(void *ptr);

ssize_t vmx_write_full(int fd, const void *buf, size_t count)
    QEMU_WARN_UNUSED_RESULT;
ssize_t vmx_send_full(int fd, const void *buf, size_t count, int flags)
    QEMU_WARN_UNUSED_RESULT;
ssize_t vmx_recv_full(int fd, void *buf, size_t count, int flags)
    QEMU_WARN_UNUSED_RESULT;

#ifndef _WIN32
int vmx_pipe(int pipefd[2]);
/* like openpty() but also makes it raw; return master fd */
int vmx_openpty_raw(int *aslave, char *pty_name);
#endif

#ifdef _WIN32
/* MinGW needs type casts for the 'buf' and 'optval' arguments. */
#define vmx_getsockopt(sockfd, level, optname, optval, optlen) \
    getsockopt(sockfd, level, optname, (void *)optval, optlen)
#define vmx_setsockopt(sockfd, level, optname, optval, optlen) \
    setsockopt(sockfd, level, optname, (const void *)optval, optlen)
#define vmx_recv(sockfd, buf, len, flags) recv(sockfd, (void *)buf, len, flags)
#define vmx_sendto(sockfd, buf, len, flags, destaddr, addrlen) \
    sendto(sockfd, (const void *)buf, len, flags, destaddr, addrlen)
#else
#define vmx_getsockopt(sockfd, level, optname, optval, optlen) \
    getsockopt(sockfd, level, optname, optval, optlen)
#define vmx_setsockopt(sockfd, level, optname, optval, optlen) \
    setsockopt(sockfd, level, optname, optval, optlen)
#define vmx_recv(sockfd, buf, len, flags) recv(sockfd, buf, len, flags)
#define vmx_sendto(sockfd, buf, len, flags, destaddr, addrlen) \
    sendto(sockfd, buf, len, flags, destaddr, addrlen)
#define vmx_send(sockfd, buf, len, flags) \
    send(sockfd, buf, len, flags)
#endif

/* Error handling.  */

void QEMU_NORETURN hw_error(const char *fmt, ...) GCC_FMT_ATTR(1, 2);

struct ParallelIOArg {
    void *buffer;
    int count;
};

typedef int (*DMA_transfer_handler) (void *opaque, int nchan, int pos, int size);

typedef uint64_t pcibus_t;

typedef struct PCIHostDeviceAddress {
    unsigned int domain;
    unsigned int bus;
    unsigned int slot;
    unsigned int function;
} PCIHostDeviceAddress;

void cpu_exec_init_all(void);

/* GETCPU save/load.  */
#ifdef CPU_SAVE_VERSION
void cpu_save(QEMUFile *f, void *opaque);
int cpu_load(QEMUFile *f, void *opaque, int version_id);
#endif

/* Unblock cpu */
void vmx_cpu_kick_self(void);

/* work queue */
struct vmx_work_item {
    struct vmx_work_item *next;
    void (*func)(void *data);
    void *data;
    int done;
    bool free;
};


/**
 * Sends a (part of) iovec down a socket, yielding when the socket is full, or
 * Receives data into a (part of) iovec from a socket,
 * yielding when there is no data in the socket.
 * The same interface as vmx_sendv_recvv(), with added yielding.
 * XXX should mark these as coroutine_fn
 */
ssize_t vmx_co_sendv_recvv(int sockfd, struct iovec *iov, unsigned iov_cnt,
                            size_t offset, size_t bytes, bool do_send);
#define vmx_co_recvv(sockfd, iov, iov_cnt, offset, bytes) \
  vmx_co_sendv_recvv(sockfd, iov, iov_cnt, offset, bytes, false)
#define vmx_co_sendv(sockfd, iov, iov_cnt, offset, bytes) \
  vmx_co_sendv_recvv(sockfd, iov, iov_cnt, offset, bytes, true)

/**
 * The same as above, but with just a single buffer
 */
ssize_t vmx_co_send_recv(int sockfd, void *buf, size_t bytes, bool do_send);
#define vmx_co_recv(sockfd, buf, bytes) \
  vmx_co_send_recv(sockfd, buf, bytes, false)
#define vmx_co_send(sockfd, buf, bytes) \
  vmx_co_send_recv(sockfd, buf, bytes, true)

typedef struct QEMUIOVector {
    struct iovec *iov;
    int niov;
    int nalloc;
    size_t size;
} QEMUIOVector;

void vmx_iovec_init(QEMUIOVector *qiov, int alloc_hint);
void vmx_iovec_init_external(QEMUIOVector *qiov, struct iovec *iov, int niov);
void vmx_iovec_add(QEMUIOVector *qiov, void *base, size_t len);
void vmx_iovec_concat(QEMUIOVector *dst,
                       QEMUIOVector *src, size_t soffset, size_t sbytes);
size_t vmx_iovec_concat_iov(QEMUIOVector *dst,
                             struct iovec *src_iov, unsigned int src_cnt,
                             size_t soffset, size_t sbytes);
bool vmx_iovec_is_zero(QEMUIOVector *qiov);
void vmx_iovec_destroy(QEMUIOVector *qiov);
void vmx_iovec_reset(QEMUIOVector *qiov);
size_t vmx_iovec_to_buf(QEMUIOVector *qiov, size_t offset,
                         void *buf, size_t bytes);
size_t vmx_iovec_from_buf(QEMUIOVector *qiov, size_t offset,
                           const void *buf, size_t bytes);
size_t vmx_iovec_memset(QEMUIOVector *qiov, size_t offset,
                         int fillc, size_t bytes);
ssize_t vmx_iovec_compare(QEMUIOVector *a, QEMUIOVector *b);
void vmx_iovec_clone(QEMUIOVector *dest, const QEMUIOVector *src, void *buf);
void vmx_iovec_discard_back(QEMUIOVector *qiov, size_t bytes);

bool buffer_is_zero(const void *buf, size_t len);

void vmx_progress_init(int enabled, float min_skip);
void vmx_progress_end(void);
void vmx_progress_print(float delta, int max);
const char *vmx_get_vm_name(void);

#define QEMU_FILE_TYPE_BIOS   0
#define QEMU_FILE_TYPE_KEYMAP 1
char *vmx_find_file(int type, const char *name);

/* OS specific functions */
void os_setup_early_signal_handling(void);
char *os_find_datadir(void);
void os_parse_cmd_args(int index, const char *optarg);

/* Convert a byte between binary and BCD.  */
static inline uint8_t to_bcd(uint8_t val)
{
    return ((val / 10) << 4) | (val % 10);
}

static inline uint8_t from_bcd(uint8_t val)
{
    return ((val >> 4) * 10) + (val & 0x0f);
}

/* compute with 96 bit intermediate result: (a*b)/c */
#ifdef CONFIG_INT128
static inline uint64_t muldiv64(uint64_t a, uint32_t b, uint32_t c)
{
    return (__int128_t)a * b / c;
}
#else
static inline uint64_t muldiv64(uint64_t a, uint32_t b, uint32_t c)
{
    union {
        uint64_t ll;
        struct {
#ifdef HOST_WORDS_BIGENDIAN
            uint32_t high, low;
#else
            uint32_t low, high;
#endif
        } l;
    } u, res;
    uint64_t rl, rh;

    u.ll = a;
    rl = (uint64_t)u.l.low * (uint64_t)b;
    rh = (uint64_t)u.l.high * (uint64_t)b;
    rh += (rl >> 32);
    res.l.high = rh / c;
    res.l.low = (((rh % c) << 32) + (rl & 0xffffffff)) / c;
    return res.ll;
}
#endif

/* Round number down to multiple */
#define QEMU_ALIGN_DOWN(n, m) ((n) / (m) * (m))

/* Round number up to multiple */
#define QEMU_ALIGN_UP(n, m) QEMU_ALIGN_DOWN((n) + (m) - 1, (m))

static inline bool is_power_of_2(uint64_t value)
{
    if (!value) {
        return 0;
    }

    return !(value & (value - 1));
}

/* round down to the nearest power of 2*/
int64_t pow2floor(int64_t value);

#include "qemu/module.h"

/*
 * Implementation of ULEB128 (http://en.wikipedia.org/wiki/LEB128)
 * Input is limited to 14-bit numbers
 */

int uleb128_encode_small(uint8_t *out, uint32_t n);
int uleb128_decode_small(const uint8_t *in, uint32_t *n);


/*
 * Hexdump a buffer to a file. An optional string prefix is added to every line
 */

void vmx_hexdump(const char *buf, FILE *fp, const char *prefix, size_t size);

/* vector definitions */
#ifdef __ALTIVEC__
#include <altivec.h>
/* The altivec.h header says we're allowed to undef these for
 * C++ compatibility.  Here we don't care about C++, but we
 * undef them anyway to avoid namespace pollution.
 */
#undef vector
#undef pixel
#undef bool
#define VECTYPE        __vector unsigned char
#define SPLAT(p)       vec_splat(vec_ld(0, p), 0)
#define ALL_EQ(v1, v2) vec_all_eq(v1, v2)
/* altivec.h may redefine the bool macro as vector type.
 * Reset it to POSIX semantics. */
#define bool _Bool
#elif defined __SSE2__
#include <emmintrin.h>
#define VECTYPE        __m128i
#define SPLAT(p)       _mm_set1_epi8(*(p))
#define ALL_EQ(v1, v2) (_mm_movemask_epi8(_mm_cmpeq_epi8(v1, v2)) == 0xFFFF)
#else
#define VECTYPE        unsigned long
#define SPLAT(p)       (*(p) * (~0UL / 255))
#define ALL_EQ(v1, v2) ((v1) == (v2))
#endif

#define BUFFER_FIND_NONZERO_OFFSET_UNROLL_FACTOR 8
static inline bool
can_use_buffer_find_nonzero_offset(const void *buf, size_t len)
{
    return (len % (BUFFER_FIND_NONZERO_OFFSET_UNROLL_FACTOR
                   * sizeof(VECTYPE)) == 0
            && ((uintptr_t) buf) % sizeof(VECTYPE) == 0);
}
size_t buffer_find_nonzero_offset(const void *buf, size_t len);

/*
 * helper to parse debug environment variables
 */
int parse_debug_env(const char *name, int max, int initial);

const char *vmx_ether_ntoa(const MACAddr *mac);

#endif

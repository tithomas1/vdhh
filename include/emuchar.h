#ifndef QEMU_CHAR_H
#define QEMU_CHAR_H

#include "qemu-common.h"
#include "qemu/queue.h"
#include "qemu/option.h"
#include "qemu/config-file.h"
#include "aio.h"
#include "qapi/qmp/qobject.h"
#include "qapi/qmp/qstring.h"
#include "qemu/main-loop.h"

/* character device */

#define CHR_EVENT_BREAK   0 /* serial break char */
#define CHR_EVENT_FOCUS   1 /* focus to this terminal (modal input needed) */
#define CHR_EVENT_OPENED  2 /* new connection established */
#define CHR_EVENT_MUX_IN  3 /* mux-focus was set to this terminal */
#define CHR_EVENT_MUX_OUT 4 /* mux-focus will move on */
#define CHR_EVENT_CLOSED  5 /* connection closed */


#define CHR_IOCTL_SERIAL_SET_PARAMS   1
typedef struct {
    int speed;
    int parity;
    int data_bits;
    int stop_bits;
} QEMUSerialSetParams;

#define CHR_IOCTL_SERIAL_SET_BREAK    2

#define CHR_IOCTL_PP_READ_DATA        3
#define CHR_IOCTL_PP_WRITE_DATA       4
#define CHR_IOCTL_PP_READ_CONTROL     5
#define CHR_IOCTL_PP_WRITE_CONTROL    6
#define CHR_IOCTL_PP_READ_STATUS      7
#define CHR_IOCTL_PP_EPP_READ_ADDR    8
#define CHR_IOCTL_PP_EPP_READ         9
#define CHR_IOCTL_PP_EPP_WRITE_ADDR  10
#define CHR_IOCTL_PP_EPP_WRITE       11
#define CHR_IOCTL_PP_DATA_DIR        12

#define CHR_IOCTL_SERIAL_SET_TIOCM   13
#define CHR_IOCTL_SERIAL_GET_TIOCM   14

#define CHR_TIOCM_CTS	0x020
#define CHR_TIOCM_CAR	0x040
#define CHR_TIOCM_DSR	0x100
#define CHR_TIOCM_RI	0x080
#define CHR_TIOCM_DTR	0x002
#define CHR_TIOCM_RTS	0x004

typedef void IOEventHandler(void *opaque, int event);

struct CharDriverState {
    QemuMutex chr_write_lock;
    void (*init)(struct CharDriverState *s);
    int (*chr_write)(struct CharDriverState *s, const uint8_t *buf, int len);
    int (*chr_sync_read)(struct CharDriverState *s,
                         const uint8_t *buf, int len);
    GSource *(*chr_add_watch)(struct CharDriverState *s, GIOCondition cond);
    void (*chr_update_read_handler)(struct CharDriverState *s);
    int (*chr_ioctl)(struct CharDriverState *s, int cmd, void *arg);
    int (*get_msgfds)(struct CharDriverState *s, int* fds, int num);
    int (*set_msgfds)(struct CharDriverState *s, int *fds, int num);
    int (*chr_add_client)(struct CharDriverState *chr, int fd);
    IOEventHandler *chr_event;
    IOCanReadHandler *chr_can_read;
    IOReadHandler *chr_read;
    void *handler_opaque;
    void (*chr_close)(struct CharDriverState *chr);
    void (*chr_accept_input)(struct CharDriverState *chr);
    void (*chr_set_echo)(struct CharDriverState *chr, bool echo);
    void (*chr_set_fe_open)(struct CharDriverState *chr, int fe_open);
    void (*chr_fe_event)(struct CharDriverState *chr, int event);
    void *opaque;
    char *label;
    char *filename;
    int be_open;
    int fe_open;
    int explicit_fe_open;
    int explicit_be_open;
    int avail_connections;
    int is_mux;
    guint fd_in_tag;
    QemuOpts *opts;
    QTAILQ_ENTRY(CharDriverState) next;
};

/**
 * @vmx_chr_alloc:
 *
 * Allocate and initialize a new CharDriverState.
 *
 * Returns: a newly allocated CharDriverState.
 */
CharDriverState *vmx_chr_alloc(void);

/**
 * @vmx_chr_new_from_opts:
 *
 * Create a new character backend from a QemuOpts list.
 *
 * @opts see qemu-config.c for a list of valid options
 * @init not sure..
 *
 * Returns: a new character backend
 */
CharDriverState *vmx_chr_new_from_opts(QemuOpts *opts,
                                    void (*init)(struct CharDriverState *s),
                                    Error **errp);

/**
 * @vmx_chr_new:
 *
 * Create a new character backend from a URI.
 *
 * @label the name of the backend
 * @filename the URI
 * @init not sure..
 *
 * Returns: a new character backend
 */
CharDriverState *vmx_chr_new(const char *label, const char *filename,
                              void (*init)(struct CharDriverState *s));

/**
 * @vmx_chr_delete:
 *
 * Destroy a character backend.
 */
void vmx_chr_delete(CharDriverState *chr);

/**
 * @vmx_chr_fe_set_echo:
 *
 * Ask the backend to override its normal echo setting.  This only really
 * applies to the stdio backend and is used by the QMP server such that you
 * can see what you type if you try to type QMP commands.
 *
 * @echo true to enable echo, false to disable echo
 */
void vmx_chr_fe_set_echo(struct CharDriverState *chr, bool echo);

/**
 * @vmx_chr_fe_set_open:
 *
 * Set character frontend open status.  This is an indication that the
 * front end is ready (or not) to begin doing I/O.
 */
void vmx_chr_fe_set_open(struct CharDriverState *chr, int fe_open);

/**
 * @vmx_chr_fe_event:
 *
 * Send an event from the front end to the back end.
 *
 * @event the event to send
 */
void vmx_chr_fe_event(CharDriverState *s, int event);

/**
 * @vmx_chr_fe_printf:
 *
 * Write to a character backend using a printf style interface.
 * This function is thread-safe.
 *
 * @fmt see #printf
 */
void vmx_chr_fe_printf(CharDriverState *s, const char *fmt, ...)
    GCC_FMT_ATTR(2, 3);

int vmx_chr_fe_add_watch(CharDriverState *s, GIOCondition cond,
                          GIOFunc func, void *user_data);

/**
 * @vmx_chr_fe_write:
 *
 * Write data to a character backend from the front end.  This function
 * will send data from the front end to the back end.  This function
 * is thread-safe.
 *
 * @buf the data
 * @len the number of bytes to send
 *
 * Returns: the number of bytes consumed
 */
int vmx_chr_fe_write(CharDriverState *s, const uint8_t *buf, int len);

/**
 * @vmx_chr_fe_write_all:
 *
 * Write data to a character backend from the front end.  This function will
 * send data from the front end to the back end.  Unlike @vmx_chr_fe_write,
 * this function will block if the back end cannot consume all of the data
 * attempted to be written.  This function is thread-safe.
 *
 * @buf the data
 * @len the number of bytes to send
 *
 * Returns: the number of bytes consumed
 */
int vmx_chr_fe_write_all(CharDriverState *s, const uint8_t *buf, int len);

/**
 * @vmx_chr_fe_read_all:
 *
 * Read data to a buffer from the back end.
 *
 * @buf the data buffer
 * @len the number of bytes to read
 *
 * Returns: the number of bytes read
 */
int vmx_chr_fe_read_all(CharDriverState *s, uint8_t *buf, int len);

/**
 * @vmx_chr_fe_ioctl:
 *
 * Issue a device specific ioctl to a backend.  This function is thread-safe.
 *
 * @cmd see CHR_IOCTL_*
 * @arg the data associated with @cmd
 *
 * Returns: if @cmd is not supported by the backend, -ENOTSUP, otherwise the
 *          return value depends on the semantics of @cmd
 */
int vmx_chr_fe_ioctl(CharDriverState *s, int cmd, void *arg);

/**
 * @vmx_chr_fe_get_msgfd:
 *
 * For backends capable of fd passing, return the latest file descriptor passed
 * by a client.
 *
 * Returns: -1 if fd passing isn't supported or there is no pending file
 *          descriptor.  If a file descriptor is returned, subsequent calls to
 *          this function will return -1 until a client sends a new file
 *          descriptor.
 */
int vmx_chr_fe_get_msgfd(CharDriverState *s);

/**
 * @vmx_chr_fe_get_msgfds:
 *
 * For backends capable of fd passing, return the number of file received
 * descriptors and fills the fds array up to num elements
 *
 * Returns: -1 if fd passing isn't supported or there are no pending file
 *          descriptors.  If file descriptors are returned, subsequent calls to
 *          this function will return -1 until a client sends a new set of file
 *          descriptors.
 */
int vmx_chr_fe_get_msgfds(CharDriverState *s, int *fds, int num);

/**
 * @vmx_chr_fe_set_msgfds:
 *
 * For backends capable of fd passing, set an array of fds to be passed with
 * the next send operation.
 * A subsequent call to this function before calling a write function will
 * result in overwriting the fd array with the new value without being send.
 * Upon writing the message the fd array is freed.
 *
 * Returns: -1 if fd passing isn't supported.
 */
int vmx_chr_fe_set_msgfds(CharDriverState *s, int *fds, int num);

/**
 * @vmx_chr_fe_claim:
 *
 * Claim a backend before using it, should be called before calling
 * vmx_chr_add_handlers(). 
 *
 * Returns: -1 if the backend is already in use by another frontend, 0 on
 *          success.
 */
int vmx_chr_fe_claim(CharDriverState *s);

/**
 * @vmx_chr_fe_claim_no_fail:
 *
 * Like vmx_chr_fe_claim, but will exit qemu with an error when the
 * backend is already in use.
 */
void vmx_chr_fe_claim_no_fail(CharDriverState *s);

/**
 * @vmx_chr_fe_claim:
 *
 * Release a backend for use by another frontend.
 *
 * Returns: -1 if the backend is already in use by another frontend, 0 on
 *          success.
 */
void vmx_chr_fe_release(CharDriverState *s);

/**
 * @vmx_chr_be_can_write:
 *
 * Determine how much data the front end can currently accept.  This function
 * returns the number of bytes the front end can accept.  If it returns 0, the
 * front end cannot receive data at the moment.  The function must be polled
 * to determine when data can be received.
 *
 * Returns: the number of bytes the front end can receive via @vmx_chr_be_write
 */
int vmx_chr_be_can_write(CharDriverState *s);

/**
 * @vmx_chr_be_write:
 *
 * Write data from the back end to the front end.  Before issuing this call,
 * the caller should call @vmx_chr_be_can_write to determine how much data
 * the front end can currently accept.
 *
 * @buf a buffer to receive data from the front end
 * @len the number of bytes to receive from the front end
 */
void vmx_chr_be_write(CharDriverState *s, uint8_t *buf, int len);


/**
 * @vmx_chr_be_event:
 *
 * Send an event from the back end to the front end.
 *
 * @event the event to send
 */
void vmx_chr_be_event(CharDriverState *s, int event);

void vmx_chr_add_handlers(CharDriverState *s,
                           IOCanReadHandler *fd_can_read,
                           IOReadHandler *fd_read,
                           IOEventHandler *fd_event,
                           void *opaque);

void vmx_chr_be_generic_open(CharDriverState *s);
void vmx_chr_accept_input(CharDriverState *s);
int vmx_chr_add_client(CharDriverState *s, int fd);
CharDriverState *vmx_chr_find(const char *name);
bool chr_is_ringbuf(const CharDriverState *chr);

QemuOpts *vmx_chr_parse_compat(const char *label, const char *filename);

void register_char_driver(const char *name, ChardevBackendKind kind,
        void (*parse)(QemuOpts *opts, ChardevBackend *backend, Error **errp));

/* add an eventfd to the qemu devices that are polled */
CharDriverState *vmx_chr_open_eventfd(int eventfd);

extern int term_escape_char;

CharDriverState *vmx_char_get_next_serial(void);


/* baum.c */
CharDriverState *chr_baum_init(void);

/* console.c */
typedef CharDriverState *(VcHandler)(ChardevVC *vc);

void register_vc_handler(VcHandler *handler);
CharDriverState *vc_init(ChardevVC *vc);
#endif

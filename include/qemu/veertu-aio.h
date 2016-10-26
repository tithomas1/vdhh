//
//  veertu-aio.h
//  Veertu VMX

#ifndef __VEERTU_AIO_H__
#define __VEERTU_AIO_H__

/* AIO request types */
#define QEMU_AIO_READ         0x0001
#define QEMU_AIO_WRITE        0x0002
#define QEMU_AIO_IOCTL        0x0004
#define QEMU_AIO_FLUSH        0x0008
#define QEMU_AIO_DISCARD      0x0010
#define QEMU_AIO_WRITE_ZEROES 0x0020
#define QEMU_AIO_TYPE_MASK \
(QEMU_AIO_READ|QEMU_AIO_WRITE|QEMU_AIO_IOCTL|QEMU_AIO_FLUSH| \
QEMU_AIO_DISCARD|QEMU_AIO_WRITE_ZEROES)

/* AIO flags */
#define QEMU_AIO_MISALIGNED   0x1000
#define QEMU_AIO_BLKDEV       0x2000

typedef struct BlockAIOCB BlockAIOCB;
typedef void BlockCompletionFunc(void *opaque, int ret);

typedef struct AIOCBInfo {
    void (*cancel_async)(BlockAIOCB *acb);
    VeertuAioContext *(*get_aio_context)(BlockAIOCB *acb);
    size_t aiocb_size;
} AIOCBInfo;

struct BlockAIOCB {
    const AIOCBInfo *aiocb_info;
    BlockDriverState *bs;
    BlockCompletionFunc *cb;
    void *opaque;
    int refcnt;
};

void *vmx_aio_get(const AIOCBInfo *aiocb_info, BlockDriverState *bs,
                   BlockCompletionFunc *cb, void *opaque);
void vmx_aio_unref(void *p);
void vmx_aio_ref(void *p);

/**
 * vmx_bh_schedule: Schedule a bottom half.
 */
void vmx_bh_schedule(QEMUBH *bh);

/**
 * vmx_bh_cancel: Cancel execution of a bottom half.
 */
void vmx_bh_cancel(QEMUBH *bh);

/**
 *vmx_bh_delete: Cancel execution of a bottom half and free its resources
 */
void vmx_bh_delete(QEMUBH *bh);

#endif /* __VEERTU_AIO_H__ */

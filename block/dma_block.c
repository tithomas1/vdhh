//
//  dma_block.c
//  Veertu VMX
#include "emudma.h"
#include "dma_block.h"
#include "emublock-backend.h"

typedef struct {
    BlockAIOCB common;
    BlockBackend *blk;
    BlockAIOCB *acb;
    VeertuSGList *sg;
    uint64_t sector_num;
    int dir;
    int sg_index;
    uint64_t sg_cur_byte;
    QEMUIOVector iov;
    QEMUBH *bh;
    DMAIOFunc *io_func;
} DMAAIOCB;

static void dma_blk_unmap(DMAAIOCB *dbs)
{
    for (int i = 0; i < dbs->iov.niov; i++) {
        dma_memory_unmap(dbs->sg->as, dbs->iov.iov[i].iov_base,
                         dbs->iov.iov[i].iov_len, dbs->dir,
                         dbs->iov.iov[i].iov_len);
    }
    vmx_iovec_reset(&dbs->iov);
}

static void dma_complete(DMAAIOCB *dbs, int ret)
{
    dma_blk_unmap(dbs);
    if (dbs->common.cb)
        dbs->common.cb(dbs->common.opaque, ret);

    vmx_iovec_destroy(&dbs->iov);
    if (dbs->bh) {
        vmx_bh_delete(dbs->bh);
        dbs->bh = NULL;
    }
    vmx_aio_unref(dbs);
}

static void continue_after_map_failure(void *opaque);

static void dma_blk_do_work(void *opaque, int ret)
{
    DMAAIOCB *dbs = (DMAAIOCB *)opaque;
    uint64_t cur_addr, cur_len;
    void *mem;
    int sectors = dbs->iov.size / BDRV_SECTOR_SIZE;

    dbs->acb = NULL;
    dbs->sector_num += sectors;

    if (dbs->sg_index == dbs->sg->nsg || ret < 0) {
        dma_complete(dbs, ret);
        return;
    }
    dma_blk_unmap(dbs);

    while (dbs->sg_index < dbs->sg->nsg) {
        cur_addr = dbs->sg->sg[dbs->sg_index].base + dbs->sg_cur_byte;
        cur_len = dbs->sg->sg[dbs->sg_index].len - dbs->sg_cur_byte;
        mem = dma_memory_map(dbs->sg->as, cur_addr, &cur_len, dbs->dir);
        if (!mem)
            break;
        vmx_iovec_add(&dbs->iov, mem, cur_len);
        dbs->sg_cur_byte += cur_len;
        if (dbs->sg_cur_byte == dbs->sg->sg[dbs->sg_index].len) {
            dbs->sg_cur_byte = 0;
            dbs->sg_index++;
        }
    }
    
    if (dbs->iov.size == 0) {
        cpu_register_map_client(dbs, continue_after_map_failure);
        return;
    }

    if (dbs->iov.size & ~BDRV_SECTOR_MASK)
        vmx_iovec_discard_back(&dbs->iov, dbs->iov.size & ~BDRV_SECTOR_MASK);

    sectors = dbs->iov.size / BDRV_SECTOR_SIZE;
    dbs->acb = dbs->io_func(dbs->blk, dbs->sector_num, &dbs->iov, sectors, dma_blk_do_work, dbs);
}

static void reschedule(void *opaque)
{
    DMAAIOCB *dbs = (DMAAIOCB *)opaque;
    
    vmx_bh_delete(dbs->bh);
    dbs->bh = NULL;
    dma_blk_do_work(dbs, 0);
}

static void continue_after_map_failure(void *opaque)
{
    DMAAIOCB *dbs = (DMAAIOCB *)opaque;
    
    dbs->bh = vmx_bh_new(reschedule, dbs);
    vmx_bh_schedule(dbs->bh);
}

static void dma_aio_cancel(BlockAIOCB *acb)
{
    DMAAIOCB *dbs = container_of(acb, DMAAIOCB, common);

    if (dbs->acb)
        blk_aio_cancel_async(dbs->acb);
}


static const AIOCBInfo dma_aiocb_info = {
    .aiocb_size         = sizeof(DMAAIOCB),
    .cancel_async       = dma_aio_cancel,
};

BlockAIOCB *dma_blk_io(
                       BlockBackend *blk, VeertuSGList *sg, uint64_t sector_num,
                       DMAIOFunc *io_func, BlockCompletionFunc *cb,
                       void *opaque, int dir)
{
    DMAAIOCB *dbs = blk_aio_get(&dma_aiocb_info, blk, cb, opaque);
    
    dbs->acb = NULL;
    dbs->blk = blk;
    dbs->sg = sg;
    dbs->sector_num = sector_num;
    dbs->sg_index = 0;
    dbs->sg_cur_byte = 0;
    dbs->dir = dir;
    dbs->io_func = io_func;
    dbs->bh = NULL;
    vmx_iovec_init(&dbs->iov, sg->nsg);
    dma_blk_do_work(dbs, 0);
    return &dbs->common;
}


static uint64_t dma_buf_rw(uint8_t *ptr, int32_t len, VeertuSGList *sg, int dir)
{
    int i = 0;
    uint64_t total_copied = 0;
    while (len) {
        ScatterGatherEntry *entry = &sg->sg[i++];
        int copy_len = MIN(entry->len, len);
        dma_memory_rw(sg->as, entry->base, ptr + total_copied, copy_len, dir);
        len -= copy_len;
        total_copied += copy_len;
    }
    return sg->size - total_copied;
}

uint64_t dma_buf_read(uint8_t *ptr, int32_t len, VeertuSGList *sg)
{
    return dma_buf_rw(ptr, len, sg, 1);
}

uint64_t dma_buf_write(uint8_t *ptr, int32_t len, VeertuSGList *sg)
{
    return dma_buf_rw(ptr, len, sg, 0);
}

BlockAIOCB *dma_blk_read(BlockBackend *blk,
                         VeertuSGList *sg, uint64_t sector,
                         void (*cb)(void *opaque, int ret), void *opaque)
{
    return dma_blk_io(blk, sg, sector, blk_aio_readv, cb, opaque, 1);
}

BlockAIOCB *dma_blk_write(BlockBackend *blk,
                          VeertuSGList *sg, uint64_t sector,
                          void (*cb)(void *opaque, int ret), void *opaque)
{
    return dma_blk_io(blk, sg, sector, blk_aio_writev, cb, opaque, 0);
}
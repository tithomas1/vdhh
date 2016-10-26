//
//  dma_block.h
//  Veertu VMX

#ifndef __DMA_BLOCK_H__
#define __DMA_BLOCK_H__

#include "emudma.h"

typedef BlockAIOCB *DMAIOFunc(BlockBackend *blk, int64_t sector_num,
                              QEMUIOVector *iov, int nb_sectors,
                              BlockCompletionFunc *cb, void *opaque);

BlockAIOCB *dma_blk_io(BlockBackend *blk,
                       VeertuSGList *sg, uint64_t sector_num,
                       DMAIOFunc *io_func, BlockCompletionFunc *cb,
                       void *opaque, int dir);
BlockAIOCB *dma_blk_read(BlockBackend *blk,
                         VeertuSGList *sg, uint64_t sector,
                         BlockCompletionFunc *cb, void *opaque);
BlockAIOCB *dma_blk_write(BlockBackend *blk,
                          VeertuSGList *sg, uint64_t sector,
                          BlockCompletionFunc *cb, void *opaque);
uint64_t dma_buf_read(uint8_t *ptr, int32_t len, VeertuSGList *sg);
uint64_t dma_buf_write(uint8_t *ptr, int32_t len, VeertuSGList *sg);

void dma_acct_start(BlockBackend *blk, BlockAcctCookie *cookie,
                    VeertuSGList *sg, enum BlockAcctType type);

#endif

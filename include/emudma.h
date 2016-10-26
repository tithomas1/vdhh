#ifndef DMA_H
#define DMA_H

#include "sglist.h"
#include "memory.h"
#include "address-spaces.h"

static int inline dma_memory_rw(VeertuAddressSpace *address_space, uint64_t address, const void *buffer, uint64_t len, int from_dev)
{
    return address_space_rw(address_space, address, buffer, len, from_dev);
}

static int inline dma_memory_read(VeertuAddressSpace *address_space, uint64_t address, const void *buffer, uint64_t len)
{
    return address_space_rw(address_space, address, buffer, len, 0);
}

static int inline dma_memory_write(VeertuAddressSpace *address_space, uint64_t address, const void *buffer, uint64_t len)
{
    return address_space_rw(address_space, address, buffer, len, 1);
}

static inline void dma_memory_unmap(VeertuAddressSpace *address_space, void *buffer, uint64_t len, int from_dev, uint64_t acc_len)
{
    address_space_unmap(address_space, buffer, len, from_dev, acc_len);
}

static inline void *dma_memory_map(VeertuAddressSpace *address_space, uint64_t address, uint64_t *len, int from_dev)
{
    void *r;
    uint64_t nlen;
    
    nlen = *len;
    r = address_space_map(address_space, address, &nlen, from_dev);
    *len = nlen;
    return r;
}

static inline void stq_le_dma(VeertuAddressSpace *address_space, uint64_t address, uint64_t v)
{
    address_space_rw(address_space, address, &v, 8, 1);
}

static inline uint64_t ldq_le_dma(VeertuAddressSpace *address_space, uint64_t address)
{
    uint64_t v;
    
    address_space_rw(address_space, address, &v, 8, 0);
    return v;
}

#endif
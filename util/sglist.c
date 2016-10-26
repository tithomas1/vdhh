//
//  sglist.c
//  Veertu VMX
//
#include "sglist.h"

void veertu_sglist_init(VeertuSGList *qsg, DeviceState *dev, int alloc_hint,
                        VeertuAddressSpace *as)
{
    qsg->sg = g_malloc(alloc_hint * sizeof(ScatterGatherEntry));
    qsg->nsg = 0;
    qsg->nalloc = alloc_hint;
    qsg->size = 0;
    qsg->as = as;
    qsg->dev = dev;
}

void veertu_sglist_add(VeertuSGList *qsg, uint64_t base, uint64_t len)
{
    if (qsg->nsg == qsg->nalloc) {
        qsg->nalloc = 2 * qsg->nalloc + 1;
        qsg->sg = g_realloc(qsg->sg, qsg->nalloc * sizeof(ScatterGatherEntry));
    }
    qsg->sg[qsg->nsg].base = base;
    qsg->sg[qsg->nsg].len = len;
    qsg->size += len;
    ++qsg->nsg;
}

void veertu_sglist_destroy(VeertuSGList *qsg)
{
    g_free(qsg->sg);
    memset(qsg, 0, sizeof(*qsg));
}
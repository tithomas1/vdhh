//
//  sglist.h
//  Veertu VMX
//
//

#ifndef __SGLIST_H__
#define __SGLIST_H__

#include <stdio.h>
#include "memory.h"
#include "address-spaces.h"
#include "hw.h"
#include "block.h"
#include "accounting.h"

typedef struct ScatterGatherEntry ScatterGatherEntry;

typedef uint64_t uint64_t;

struct ScatterGatherEntry {
    uint64_t base;
    uint64_t len;
};

typedef struct VeertuSGList {
    ScatterGatherEntry *sg;
    int nsg;
    int nalloc;
    size_t size;
    DeviceState *dev;
    VeertuAddressSpace *as;
} VeertuSGList;

void veertu_sglist_init(VeertuSGList *qsg, DeviceState *dev, int alloc_hint,
                      VeertuAddressSpace *as);
void veertu_sglist_add(VeertuSGList *qsg, uint64_t base, uint64_t len);
void veertu_sglist_destroy(VeertuSGList *qsg);


#endif

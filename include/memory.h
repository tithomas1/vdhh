#ifndef VEERTUMEM_H
#define VEERTUMEM_H


#include <stdint.h>

#define DIRTY_MEMORY_VGA 0
#define DIRTY_MEMORY_MIGRATION 1
#define DIRTY_MEMORY_CODE 2
#define DIRTY_MEMORY_NUM 3
#include "qemu-common.h"
#include "hwaddr.h"
#include "cpu-common.h"
#include "qemu/queue.h"
#include "typeinfo.h"

ram_addr_t vmx_ram_alloc_from_ptr(ram_addr_t size, void *host,
                                   VeertuMemArea *mr, Error **errp);
ram_addr_t vmx_ram_alloc(ram_addr_t size, VeertuMemArea *mr, Error **errp);
ram_addr_t vmx_ram_alloc_resizeable(ram_addr_t size, ram_addr_t maxsz,
                                     void (*resized)(const char*,
                                                     uint64_t length,
                                                     void *host),
                                     VeertuMemArea *mr, Error **errp);
void vmx_ram_free_from_ptr(ram_addr_t addr);
void vmx_ram_free(ram_addr_t addr);
void *vmx_get_ram_ptr(ram_addr_t addr);
int vmx_ram_resize(ram_addr_t base, ram_addr_t newsize, Error **errp);

void address_space_init_dispatch(VeertuAddressSpace *address_space);
void address_space_destroy_dispatch(VeertuAddressSpace *address_space);
int mem_area_is_valid_access(VeertuMemArea *area, uint64_t addr, int size, int write);

static int inline cpu_physical_memory_get_diry_flat(uint64_t addr, int client)
{
    return 1;
}
static int inline cpu_physical_memory_is_clean(uint64_t addr)
{
    return 1;
}
static int inline cpu_physical_memory_range_includes_clean(uint64_t start, uint64_t size)
{
    return 1;
}
static void inline cpu_physical_memory_set_dirty_flag(uint64_t addr, int client)
{
}
static void inline cpu_physical_memory_set_dirty_range_nocode(uint64_t start, uint64_t size)
{
}
static void inline cpu_physical_memory_set_dirty_range(uint64_t start, uint64_t size)
{
}
static void inline cpu_physical_memory_set_dirty_lebitmap(uint64_t *bitmap, uint64_t start, uint64_t pages)
{
}
static void inline cpu_physical_memory_clear_dirty_range_type(uint64_t start, uint64_t size, int client)
{
}
static void inline cpu_physical_memory_clear_dirty_range(uint64_t start, uint64_t size)
{
}

void cpu_physical_memory_reset_dirty(ram_addr_t start, ram_addr_t length,
                                     unsigned client);

typedef struct MemAreaOps MemAreaOps;
typedef struct MemAreaMmio MemAreaMmio;

struct MemAreaMmio {
    CPUWriteMemoryFunc *write[3];
    CPUReadMemoryFunc *read[3];
};

struct MemAreaOps {
    MemAreaMmio old_mmio;
    int endianness;
    uint64_t (*read)(void *opaque, uint64_t addr, unsigned size);
    void (*write)(void *opaque, uint64_t addr, uint64_t data, unsigned size);
    
    struct {
        int (*accepts)(void *opaque, uint64_t addr, unsigned size, int is_write);
        int min_access_size;
        int max_access_size;
        int unaligned;
    }valid;
    
    struct {
        int min_access_size;
        int max_access_size;
        int unaligned;
    }impl;
};



#define TYPE_FN 1
#define TYPE_RAM 2
#define TYPE_ROM 3
#define TYPE_ALIAS 4

struct MemAreaSection {
    VeertuAddressSpace *address_space;
    VeertuMemArea *mr;
    uint64_t offset_within_region;
    uint64_t offset_within_address_space;
    uint64_t size;
    int readonly;
};

struct VeertuMemArea {
    VeertuType pure_junk;
    void *opaque;
    int type;
    uint64_t addr;
    uint64_t ram_addr;
    uint64_t size;
    int subpage;
    int readonly;
    int enabled;
    uint64_t align;
    VeertuMemArea *alias;
    VeertuMemArea *father;
    QTAILQ_HEAD(child ,VeertuMemArea) child;
    QTAILQ_ENTRY(VeertuMemArea) child_link;
    const char *name;
    int dont_flush;
    uint64_t alias_offset;
    int priority;
    const MemAreaOps *ops;
    
};

struct MemoryCallbacks {
    void (*region_add)(MemoryCallbacks *callbacks, MemAreaSection *section);
    void (*region_del)(MemoryCallbacks *callbacks, MemAreaSection *section);
    void (*begin)(MemoryCallbacks *callbacks);
    void (*commit)(MemoryCallbacks *callbacks);
    int priority;
    VeertuAddressSpace *address_space;
    QTAILQ_ENTRY(MemoryCallbacks) link;
};

struct VeertuAddressSpace {
    VeertuMemArea *root;
    char * name;
    void *current_mappings;
    MemoryCallbacks dispatch_listener;
    struct AddressSpaceDispatch *dispatch;
    struct AddressSpaceDispatch *next_dispatch;
    QTAILQ_ENTRY(VeertuAddressSpace) link;
};

void memory_area_init(VeertuMemArea *mem_area, char *name, uint64_t size);
void memory_area_init_io(VeertuMemArea *mem_area, VeertuType *owner, MemAreaOps *mem_ops, void *opaque, char * name, uint64_t size);
void mem_area_init_ram(VeertuMemArea *area, char *name, uint64_t size, Error *err);
void mem_area_init_resizeable_ram(VeertuMemArea *area, char *name, uint64_t size,
                                  uint64_t max, void (*resize)(const char *, uint64_t, void *),
                                  Error *err);
void mem_area_init_alias(VeertuMemArea *area, char *name, VeertuMemArea *p_area, uint64_t offset, uint64_t size);
void mem_area_init_rom_device(VeertuMemArea *area, MemAreaOps *ops, void *opaque, char *name, uint64_t size, Error *err);
uint64_t mem_area_get_size(VeertuMemArea *area);
int mem_area_is_ram(VeertuMemArea *area);
char *mem_area_get_name(VeertuMemArea *area);
int memory_area_is_logging(VeertuMemArea *area);
int memory_area_is_rom(VeertuMemArea *area);
void *memory_area_get_ram_ptr(VeertuMemArea *mr);
MemAreaSection memory_area_find(VeertuMemArea *mem_area, uint64_t addr, uint64_t size);
void memory_area_reset_dirty(VeertuMemArea *area, uint64_t addr, uint64_t size, unsigned client);
void mem_area_set_readonly(VeertuMemArea *area, int readonly, int flush);
void mem_area_add_child(VeertuMemArea *area, uint64_t offset, VeertuMemArea *child);
void mem_area_add_child_overlap(VeertuMemArea *area, uint64_t offset, VeertuMemArea *child, int prio);
uint64_t mem_area_get_ram_addr(VeertuMemArea *area);
void mem_are_del_child(VeertuMemArea *area, VeertuMemArea *child);
void mem_area_set_enable(VeertuMemArea *area, int enabled, int flush);
void mem_area_set_addr(VeertuMemArea *area, uint64_t addr);
void memory_area_set_size(VeertuMemArea *mem_area, uint64_t size);
void mem_area_set_alias_offset(VeertuMemArea *area, uint64_t offset);
int is_addr_in_mem_area(VeertuMemArea *area, uint64_t addr);
void veertu_mem_referesh();
void memory_callbacks_register(MemoryCallbacks *callbacks, VeertuAddressSpace *address_space);
void memory_callbacks_unregister(MemoryCallbacks *callbacks);
void veertu_address_space_init(VeertuAddressSpace *address_space, VeertuMemArea *root_area, char * name);
void veertu_address_space_destroy(VeertuAddressSpace *address_space);

bool address_space_rw(VeertuAddressSpace *address_space, hwaddr addr, uint8_t *buf, int len, bool is_write);
bool address_space_write(VeertuAddressSpace *address_space, uint64_t addr, const uint8_t *buf, int len);
bool address_space_read(VeertuAddressSpace *address_space, uint64_t addr, uint8_t *buf, int len);
bool address_space_memset(VeertuAddressSpace *as, hwaddr addr, const uint8_t value, int len);
VeertuMemArea *address_space_translate(VeertuAddressSpace *address_space, uint64_t addr, uint64_t *xlat, uint64_t *len, bool is_write);
bool address_space_access_valid(VeertuAddressSpace *address_space, uint64_t addr, int len, bool is_write);
void *address_space_map(VeertuAddressSpace *address_space, uint64_t addr, uint64_t *plen, bool is_Write);
void address_space_unmap(VeertuAddressSpace *address_space, void *buf, uint64_t len, int is_write, uint64_t access_len);


#endif

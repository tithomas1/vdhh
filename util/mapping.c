#include "memory.h"
#include "address-spaces.h"
#include "sysemu.h"
#include "typeinfo.h"
#include "ioport.h"

#define VEERTU_MEMORY "VeertuMem"

QTAILQ_HEAD(memory_callbacks, MemoryCallbacks) memory_callbacks = QTAILQ_HEAD_INITIALIZER(memory_callbacks);
QTAILQ_HEAD(, VeertuAddressSpace) veertu_address_spaces = QTAILQ_HEAD_INITIALIZER(veertu_address_spaces);

void mem_area_ops_add(VeertuAddressSpace *address_space, MemAreaSection *section)
{
    MemoryCallbacks *walk;
    
    QTAILQ_FOREACH(walk, &memory_callbacks, link) {
        if (walk->region_add && walk->address_space == section->address_space)
            walk->region_add(walk, section);
    }
}

void mem_area_ops_del(VeertuAddressSpace *address_space, MemAreaSection *section)
{
    MemoryCallbacks *walk;
    
    QTAILQ_FOREACH(walk, &memory_callbacks, link) {
        if (walk->region_del && walk->address_space == section->address_space)
            walk->region_del(walk, section);
    }
}


struct Area {
    VeertuMemArea *area;
    uint64_t start;
    uint64_t size;
    uint64_t offset_in_region;
    int readonly;
};

struct MappingAreas {
    struct Area *areas;
    int count;
};

void mapping_areas_init(struct MappingAreas *areas)
{
    areas->count = 0;
    areas->areas = g_malloc(sizeof(struct Area) * 200);
}

void mapping_areas_insert(struct MappingAreas *areas, int index, struct Area *area)
{
    memmove(areas->areas + index + 1, areas->areas + index, sizeof(struct Area) * (areas->count - index));
    areas->areas[index] = *area;
    areas->count++;
}
static void __create_memory_areas_insert(struct MappingAreas *areas, VeertuMemArea *mr, uint64_t base, uint64_t  a_start, uint64_t a_size)
{
    int x;
    struct Area area;
    uint64_t offset;
    uint64_t size;
    uint64_t left;
    
    area.area = mr;
    area.readonly = 0;
    
    offset = a_start - base;
    base = a_start;
    left = a_size;
    
    for (x = 0; x < areas->count && left; ++x) {
        if (base < areas->areas[x].start) {
            size = MIN(areas->areas[x].start - base, left);
            area.start = base;
            area.size = size;
            area.offset_in_region = offset;
            
            mapping_areas_insert(areas, x, &area);
            
            base += size;
            left -= size;
            offset += size;
        }
        if (base <= (areas->areas[x].start + areas->areas[x].size)) {
            size = MIN((base + left), areas->areas[x].start + areas->areas[x].size) - base;
            left -= size;
            offset += size;
            base += size;
        }
    }
    
    if (left) {
        area.start = base;
        area.size = left;
        area.offset_in_region = offset;
        mapping_areas_insert(areas, x, &area);
    }
}

static void __create_memory_areas(VeertuMemArea *area, struct MappingAreas *areas, uint64_t base,
                                  uint64_t a_start, uint64_t a_size)
{
    VeertuMemArea *subregion;
    
    if (!area->enabled) {
        return;
    }
    base += area->addr;
    
    if (!(((base >= a_start) && (base < (a_start + a_size))) ||
        ((a_start >= base) && (a_start < (base + area->size)))))
        return;
    
    a_start = MAX(base, a_start);
    a_size = MIN(area->size, a_size);
    
    if (area->alias) {
        base -= (area->alias->addr + area->alias_offset);
        __create_memory_areas(area->alias, areas, base, a_start, a_size);
        return;
    }
    
    QTAILQ_FOREACH(subregion, &area->child, child_link)
        __create_memory_areas(subregion, areas, base, a_start, a_size);
    
    __create_memory_areas_insert(areas, area, base, a_start, a_size);
}

struct MappingAreas *memory_perform_updates(VeertuMemArea *mem_area)
{
    struct MappingAreas *areas;
    
    areas = g_malloc(sizeof(struct MappingAreas));
    mapping_areas_init(areas);
    
    if (mem_area)
        __create_memory_areas(mem_area, areas, 0, 0, UINT64_MAX);
    
    return areas;
}

static void address_space_del(VeertuAddressSpace *as,
                              struct MappingAreas *old_mapping,
                              struct MappingAreas *new_mapping)
{
    int x;
    int y;
    struct Area *old, *new;
    int del = 0;
    for (x = 0; x < old_mapping->count; ++x)
    {
        old = &old_mapping->areas[x];
        y = 0;
        del = 1;
        for(y = 0; y < new_mapping->count; ++y) {
            new = &new_mapping->areas[y];
            if (old->area == new->area &&
                old->start == new->start &&
                old->size == new->size &&
                old->offset_in_region == new->offset_in_region &&
                old->readonly == new->readonly) {
                del = 0;
                break;
            }
        }
        if (del) {
            MemAreaSection mem_region;
            mem_region.address_space = as;
            mem_region.mr = old->area;
            mem_region.offset_within_region= old->offset_in_region;
            mem_region.offset_within_address_space = old->start;
            mem_region.readonly = old->readonly;
            mem_region.size = old->size;
            mem_area_ops_del(as, &mem_region);
        }
    }
}

static void address_space_add(VeertuAddressSpace *as,
                              struct MappingAreas *old_mapping,
                              struct MappingAreas *new_mapping)
{
    unsigned inew;
    struct Area *new;
    
    inew = 0;
    while (inew < new_mapping->count)
    {
        MemAreaSection mem_region;
        new = &new_mapping->areas[inew];
        
        
        mem_region.address_space = as;
        mem_region.mr = new->area;
        mem_region.offset_within_region= new->offset_in_region;
        mem_region.offset_within_address_space = new->start;
        mem_region.readonly = new->readonly;
        mem_region.size = new->size;
        mem_area_ops_add(as, &mem_region);
        
        inew++;
    }
}

void update_memor_mappings(VeertuAddressSpace *address_space)
{
    struct MappingAreas *new = memory_perform_updates(address_space->root);
    struct MappingAreas *old = address_space->current_mappings;
    
    address_space_del(address_space, old, new);
    address_space_add(address_space, old, new);
    
    address_space->current_mappings = new;
}

void veertu_mem_referesh()
{
    MemoryCallbacks *walk;
    VeertuAddressSpace *address_space_walk;
    
    QTAILQ_FOREACH(walk, &memory_callbacks, link)
    if (walk->begin)
        walk->begin(walk);
    
    QTAILQ_FOREACH(address_space_walk, &veertu_address_spaces, link)
        update_memor_mappings(address_space_walk);
    
    QTAILQ_FOREACH(walk, &memory_callbacks, link)
    if (walk->begin)
        walk->commit(walk);
}

void memory_area_init(VeertuMemArea *mem_area, char *name, uint64_t size)
{
    vtype_init(mem_area, sizeof(VeertuMemArea), VEERTU_MEMORY);
    mem_area->name = g_strdup(name);
    mem_area->size = size;
}

extern const MemAreaOps do_nothing_ops;

void mem_area_initfn(VeertuType *obj)
{
    VeertuMemArea *area = (VeertuMemArea *)obj;
    
    area->type = TYPE_FN;
    area->enabled = 1;
    area->ops = &do_nothing_ops;
    QTAILQ_INIT(&area->child);
}

static bool do_nothing_func(void *opauqe, uint64_t addr, unsigned size, bool is_write)
{
    return 0;
}

const MemAreaOps do_nothing_ops = {
    .valid.accepts = do_nothing_func,
};

int mem_area_is_valid_access(VeertuMemArea *area, uint64_t addr, int size, int write)
{
    return (!area->ops->valid.accepts || area->ops->valid.accepts(area->opaque, addr, size, write));
}

int memory_area_io_write(VeertuMemArea *area, uint64_t addr, uint64_t data, int size)
{
    if (!mem_area_is_valid_access(area, addr, size, 1))
        return 1;
    
    if (!area->ops->write) {
        switch (size) {
            case 1:
                area->ops->old_mmio.write[0](area->opaque, addr, (uint8_t)data);
                break;
            case 2:
                area->ops->old_mmio.write[1](area->opaque, addr, (uint16_t)data);
                break;
            case 4:
                area->ops->old_mmio.write[2](area->opaque, addr, (uint32_t)data);
                break;
        }
    } else {
        int count = 1;
        int x;
        int shift = 0;
        if (area->ops->impl.max_access_size && size > area->ops->impl.max_access_size) {
            count = size / area->ops->impl.max_access_size;
            size = area->ops->impl.max_access_size;
            shift = size * 8;
        }
        for (x = 0; x < count ;++x) {
        switch (size) {
            case 1:
                area->ops->write(area->opaque, addr, (uint8_t)data, size);
                break;
            case 2:
                area->ops->write(area->opaque, addr, (uint16_t)data, size);
                break;
            case 4:
                area->ops->write(area->opaque, addr, (uint32_t)data, size);
                break;
            case 8:
                area->ops->write(area->opaque, addr, data, size);
                break;
        }
        data = data >> shift;
        addr += size;
        }
    }
    
    return 0;
}

int memory_area_io_read(VeertuMemArea *area, uint64_t addr, uint64_t *data, int size)
{
    if (!mem_area_is_valid_access(area, addr, size, 0))
        return 1;
    
    if (!area->ops->read) {
        switch (size) {
            case 1:
                *data = area->ops->old_mmio.read[0](area->opaque, addr);
                break;
            case 2:
                *data = area->ops->old_mmio.read[1](area->opaque, addr);
                break;
            case 4:
                *data = area->ops->old_mmio.read[2](area->opaque, addr);
                break;
        }
    } else {
        int count = 1;
        int x;
        int shift = 0;
        *data = 0;
        
        if (area->ops->impl.max_access_size && size > area->ops->impl.max_access_size) {
            count = size / area->ops->impl.max_access_size;
            size = area->ops->impl.max_access_size;
            shift = size * 8;
        }
        
        for (x = 0; x < count ;++x) {
            switch (size) {
                case 1:
                    *data |= (area->ops->read(area->opaque, addr, size) << (shift * x));
                    break;
                case 2:
                    *data |= (area->ops->read(area->opaque, addr, size) << (shift * x));
                    break;
                case 4:
                    *data |= (area->ops->read(area->opaque, addr, size) << (shift * x));
                    break;
                case 8:
                    *data |= (area->ops->read(area->opaque, addr, size) << (shift * x));
                    break;
            }
            addr += size;
        }
    }
    
    return 0;
}

void memory_area_init_io(VeertuMemArea *mem_area, VeertuType *owner, MemAreaOps *mem_ops, void *opaque, char * name, uint64_t size)
{
    memory_area_init(mem_area, name, size);
    mem_area->opaque = opaque;
    mem_area->ops = mem_ops;
}

void mem_area_init_ram(VeertuMemArea *area, char *name, uint64_t size, Error *err)
{
    memory_area_init(area, name ,size);
    area->type = TYPE_RAM;
    area->ram_addr = vmx_ram_alloc(size, area, err);
}

void mem_area_init_resizeable_ram(VeertuMemArea *area, char *name, uint64_t size,
                                  uint64_t max, void (*resize)(const char *, uint64_t, void *),
                                  Error *err)
{
    memory_area_init(area, name ,size);
    area->type = TYPE_RAM;
    area->ram_addr = vmx_ram_alloc_resizeable(size, max, resize, area, err);
}

void mem_area_init_alias(VeertuMemArea *area, char *name, VeertuMemArea *p_area, uint64_t offset, uint64_t size)
{
    memory_area_init(area, name, size);
    area->type = TYPE_ALIAS;
    area->alias = p_area;
    area->alias_offset = offset;
}

void mem_area_init_rom_device(VeertuMemArea *area, MemAreaOps *ops, void *opaque, char *name, uint64_t size, Error *err)
{
    memory_area_init(area, name, size);
    area->type = TYPE_ROM;
    area->ram_addr = vmx_ram_alloc(size, area, err);
    area->ops = ops;
}

void mem_area_finial(VeertuType *obj)
{
    VeertuMemArea *area = (VeertuMemArea *)obj;
    switch(area->type) {
        case TYPE_RAM:
        case TYPE_ALIAS:
            vmx_ram_free(area->ram_addr);
            break;
        case TYPE_ROM:
            vmx_ram_free(area->ram_addr & TARGET_PAGE_MASK);
            break;
    }
    g_free(area->name);
}

uint64_t mem_area_get_size(VeertuMemArea *area)
{
    return area->size;
}

char *mem_area_get_name(VeertuMemArea *area)
{
    return area->name;
}

int mem_area_is_ram(VeertuMemArea *area)
{
    return area->type == TYPE_RAM;
}

int memory_area_is_logging(VeertuMemArea *area)
{
    return 0;
}

int memory_area_is_rom(VeertuMemArea *area)
{
    return area->type == TYPE_ROM;
}

void mem_area_set_readonly(VeertuMemArea *area, int readonly, int flush)
{
    area->readonly = readonly;
    if (flush)
        veertu_mem_referesh();
}

void *memory_area_get_ram_ptr(VeertuMemArea *area)
{
    return area->alias ? vmx_get_ram_ptr(area->alias->ram_addr) : vmx_get_ram_ptr(area->ram_addr);
}

void memory_area_reset_dirty(VeertuMemArea *mr, hwaddr addr, hwaddr size, unsigned client)
{
}

void mem_area_order_childs(VeertuMemArea *child)
{
    VeertuMemArea *walk;
    VeertuMemArea *father;
    
    father = child->father;
    
    QTAILQ_FOREACH(walk, &father->child, child_link) {
        if (child->priority >= walk->priority) {
            QTAILQ_INSERT_BEFORE(walk, child, child_link);
            veertu_mem_referesh();
            return;
        }
    }
    
    QTAILQ_INSERT_TAIL(&father->child, child, child_link);
    veertu_mem_referesh();
}

void mem_area_add_child_overlap(VeertuMemArea *area, uint64_t offset, VeertuMemArea *child, int prio)
{
    child->priority = prio;
    child->father = area;
    child->addr = offset;
    mem_area_order_childs(child);
}


void mem_area_add_child(VeertuMemArea *area, uint64_t offset, VeertuMemArea *child)
{
    child->priority = 0;
    child->father = area;
    child->addr = offset;
    mem_area_order_childs(child);
}


void mem_area_set_enable(VeertuMemArea *area, int enabled, int flush)
{
    area->enabled = enabled;
    if (flush)
        veertu_mem_referesh();
}

void mem_are_del_child(VeertuMemArea *area, VeertuMemArea *child)
{
    child->father = NULL;
    QTAILQ_REMOVE(&area->child, child, child_link);
    veertu_mem_referesh();
}

void memory_area_set_size(VeertuMemArea *mem_area, uint64_t size)
{
    mem_area->size = size;
    veertu_mem_referesh();
}

void mem_area_set_addr(VeertuMemArea *area, uint64_t addr)
{
    area->addr = addr;
}

void mem_area_set_alias_offset(VeertuMemArea *area, uint64_t offset)
{
    area->alias_offset = offset;
    veertu_mem_referesh();
}

uint64_t mem_area_get_ram_addr(VeertuMemArea *area)
{
    return area->ram_addr;
}

int is_addr_in_mem_area(VeertuMemArea *area, uint64_t addr)
{
    return false;
}

MemAreaSection memory_area_find(VeertuMemArea *mem_area, uint64_t addr, uint64_t size)
{
    MemAreaSection r;
    memset(&r, 0, sizeof(r));
    return r;
}

void memory_callbacks_register(MemoryCallbacks *callbacks, VeertuAddressSpace *address_space)
{
    callbacks->address_space = address_space;
    QTAILQ_INSERT_TAIL(&memory_callbacks, callbacks, link);
}

void memory_callbacks_unregister(MemoryCallbacks *callbacks)
{
    QTAILQ_REMOVE(&memory_callbacks, callbacks, link);
}
void veertu_address_space_init(VeertuAddressSpace *address_space, VeertuMemArea *root_area, char *name)
{
    VeertuAddressSpace *as = address_space;
    struct MappingAreas *areas;
    address_space->root = root_area;
    QTAILQ_INSERT_TAIL(&veertu_address_spaces, address_space, link);
    address_space->name = g_strdup(name);
    areas = address_space->current_mappings = g_malloc(sizeof (struct MappingAreas));
    areas->count = 0;
    
    address_space_init_dispatch(address_space);
    veertu_mem_referesh();
}

void veertu_address_space_destroy(VeertuAddressSpace *address_space)
{
    struct MappingAreas *areas;
    
    QTAILQ_REMOVE(&veertu_address_spaces, address_space, link);
    address_space_destroy_dispatch(address_space);
    veertu_mem_referesh();
    areas = address_space->current_mappings;
    g_free(areas->areas);
    g_free(areas);
    g_free(address_space->name);
}

VeertuTypeInfo mem_area_info = {
    .name = VEERTU_MEMORY,
    .parent = VtypeBase,
    .instance_size = sizeof(VeertuMemArea),
    .instance_init = mem_area_initfn,
};

void veertu_mem_init()
{
    register_type_internal(&mem_area_info);
}

type_init(veertu_mem_init);

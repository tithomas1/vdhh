#include "qemu-common.h"
#include "typeinfo.h""

#define MAX_DEVS 512

typedef struct types_regs {
    VeertuTypeClass *types[MAX_DEVS];
} types_regs;

static int device_index = 0;

types_regs _types;

void insert_to_type(VeertuTypeClass *type)
{
    _types.types[device_index++] = type;
}

VeertuTypeClass *get_type(char *name)
{
    int x;
    
    for (x = 0; x < device_index; ++x)
        if (!strcmp(name, _types.types[x]->name))
            return _types.types[x];
    
    return NULL;
}

VeertuTypeClass *new_type(VeertuTypeInfo *info)
{
    VeertuTypeClass *type = g_malloc0(sizeof(VeertuTypeClass));
    
    type->parent = g_strdup(info->parent);
    type->name = g_strdup(info->name);
    type->instance_size = info->instance_size;
    type->class_size= info->class_size;
    type->class_data = info->class_data;
    type->class_init = info->class_init;
    type->class_size = info->class_size;
    type->instance_init = info->instance_init;

    return type;
}

VeertuTypeClass *get_parent_type(VeertuTypeClass *impl)
{
    impl->parent_link = get_type(impl->parent);
    return impl->parent_link;
}

VeertuTypeClass *register_type_internal(VeertuTypeInfo *type)
{
    VeertuTypeClass *impl = new_type(type);
    insert_to_type(impl);
    return impl;
}

size_t get_obj_type_size(VeertuTypeClass *impl)
{
    if (impl && impl->instance_size)
        return impl->instance_size;
    if (impl && impl->parent)
        return get_obj_type_size(get_parent_type(impl));
    
    
    return sizeof(VeertuTypeClassHold);
}

size_t get_class_type_size(VeertuTypeClass *impl)
{
    if (impl && impl->class_size)
        return impl->class_size;
    if (impl && impl->parent) {
        VeertuTypeClass* ptype = get_parent_type(impl);
        return get_class_type_size(ptype);
    }

    return sizeof(VeertuTypeClassHold);
}

void init_type(VeertuTypeClass *type)
{
    VeertuTypeClass *father = get_parent_type(type);
    
    if (type->class)
        return;
    
    type->instance_size = get_obj_type_size(type);
    type->class_size = get_class_type_size(type);
    type->class = g_malloc0(get_class_type_size(type));
    father = get_parent_type(type);
    if (father) {
        init_type(father);
        memcpy(type->class, father->class, get_class_type_size(father));
    }
    
    type->class->type = type;
    if (type->class_init)
        type->class_init(type->class, type->class_data);
}

void vtype_init_with_type(VeertuType *object, VeertuTypeClass *type)
{
    if (!type)
        return;
    
    if (type->parent)
        vtype_init_with_type(object, get_parent_type(type));
    if (type->instance_init)
        type->instance_init(object);
}

void vtype_init(void *data, int size, char *name)
{
    VeertuType *object = data;
    
    init_type(get_type(name));
    memset(data, 0, get_type(name)->instance_size);
    object->class = get_type(name)->class;
    vtype_init_with_type(object, get_type(name));
}


VeertuType *vtype_new(char *name)
{
    VeertuType *_type;
    
    _type = g_malloc0(get_type(name)->instance_size);
    vtype_init(_type, get_type(name)->instance_size, name);
    return _type;
}

char *get_typename(VeertuType *type)
{
    return type->class->type->name;
}

VeertuTypeClassHold *get_type_class(char *name)
{
    return get_type(name)->class;
}


void global_init_type()
{
    int x;
    
    for (x = 0; x < device_index; ++x)
        init_type(_types.types[x]);
}




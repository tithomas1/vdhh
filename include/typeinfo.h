#ifndef TYPEINFO_H
#define TYPEINFO_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <glib.h>

#define VtypeBase "Base"


struct VeertuTypeClass;
typedef struct VeertuTypeClass *Type;


struct VeertuType;
typedef struct VeertuType VeertuType;


struct VeertuTypeClassHold {
    Type type;
};

typedef struct VeertuTypeClassHold VeertuTypeClassHold;
struct VeertuType {
    VeertuTypeClassHold *class;
    VeertuType *father;
};

struct VeertuTypeInfo {
    char *name;
    char *parent;
    size_t instance_size;
    size_t class_size;
    void (*class_init)(VeertuTypeClassHold *class, void *data);
    void *class_data;
    void (*instance_init)(VeertuType *type);
};

#define VeertuTypeHold(___type) ((VeertuType *)(___type))
#define VeertuTypeClassHold(__type) ((VeertuTypeClassHold *)(__type))

typedef struct VeertuInterfaceClass VeertuInterfaceClass;
struct VeertuInterfaceClass {
    VeertuTypeClassHold parent_class;
};

VeertuType *vtype_new(char *name);
void vtype_init(void *data, int size, char *name);
char *get_typename(VeertuType *type);
typedef struct VeertuTypeInfo VeertuTypeInfo;
struct VeertuTypeClass *register_type_internal(VeertuTypeInfo *type);
VeertuTypeClassHold *get_type_class(char *name);
void global_init_type();



typedef struct VeertuTypeClass VeertuTypeClass;

struct VeertuTypeClass {
    char *name;
    char *parent;
    VeertuTypeClass *parent_link;
    VeertuTypeClassHold *class;
    size_t class_size;
    void (*class_init)(VeertuTypeClassHold *class, void *data);
    size_t instance_size;
    void *class_data;
    void (*instance_init)(VeertuType *type);
};

#endif

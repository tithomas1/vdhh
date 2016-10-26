#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "qemu/module.h"
#include "qemu/queue.h"
#include "qemu-common.h"

typedef struct modue_init_list_entry {
    QTAILQ_ENTRY(modue_init_list_entry) entry_node;
    void (*func)(void);
} modue_init_list_entry;

static QTAILQ_HEAD(, modue_init_list_entry) TypesList[4];

static int first = 0;

void veertu_moudle_call_init(int init_type)
{
    modue_init_list_entry *entry;
    QTAILQ_HEAD(, modue_init_list_entry) *typelist;
    
    if (!first) {
        first = 1;
        int x;
        
        for (x = 0; x < 4; ++x)
            QTAILQ_INIT(&TypesList[x]);
    }
    
    typelist = &TypesList[init_type];
    QTAILQ_FOREACH(entry, typelist, entry_node)
        entry->func();
    
}

void veertu_register_module(void (*func)(void), int init_type)
{
    modue_init_list_entry *entry;
    QTAILQ_HEAD(, modue_init_list_entry) *typelist;
    
    if (!first) {
        first = 1;
        int x;
        
        for (x = 0; x < 4; ++x)
            QTAILQ_INIT(&TypesList[x]);
    }
    
    entry = g_malloc0(sizeof(modue_init_list_entry));
    entry->func = func;
    typelist = &TypesList[init_type];
    QTAILQ_INSERT_TAIL(typelist, entry, entry_node);
}
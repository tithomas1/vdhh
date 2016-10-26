#ifndef EXEC_MEMORY_H
#define EXEC_MEMORY_H

#include "memory.h"
VeertuMemArea *get_system_memory(void);
VeertuMemArea *get_system_io(void);
extern VeertuAddressSpace address_space_memory;
extern VeertuAddressSpace address_space_io;

#endif

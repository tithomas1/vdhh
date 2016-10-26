#ifndef _MODULE_H
#define _MODULE_H

void veertu_register_module(void (*func)(void), int init_type);
void veertu_moudle_call_init(int init_type);

#define type_init(func) static void __attribute__((constructor)) bla##func(void) { veertu_register_module(func, 3); }
#define qapi_init(func) static void __attribute__((constructor)) bla##func(void) { veertu_register_module(func, 2); }
#define machine_init(func) static void __attribute__((constructor)) bla##func(void) { veertu_register_module(func, 1); }
#define block_init(func) static void __attribute__((constructor)) bla##func(void) { veertu_register_module(func, 0); }


#endif
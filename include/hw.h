/* Declarations for use by hardware emulation.  */
#ifndef QEMU_HW_H
#define QEMU_HW_H

#include "qemu-common.h"

#if !defined(CONFIG_USER_ONLY) && !defined(NEED_CPU_H)
//#include "cpu-common.h"
#endif

#include "ioport.h"
#include "irq.h"
#include "aio.h"
#include "vmstate.h"
#include "qemu/log.h"

//#ifdef NEED_CPU_H
#if TARGET_LONG_BITS == 64
#define vmx_put_betl vmx_put_be64
#define vmx_get_betl vmx_get_be64
#define vmx_put_betls vmx_put_be64s
#define vmx_get_betls vmx_get_be64s
#define vmx_put_sbetl vmx_put_sbe64
#define vmx_get_sbetl vmx_get_sbe64
#define vmx_put_sbetls vmx_put_sbe64s
#define vmx_get_sbetls vmx_get_sbe64s
/*#else
#define vmx_put_betl vmx_put_be32
#define vmx_get_betl vmx_get_be32
#define vmx_put_betls vmx_put_be32s
#define vmx_get_betls vmx_get_be32s
#define vmx_put_sbetl vmx_put_sbe32
#define vmx_get_sbetl vmx_get_sbe32
#define vmx_put_sbetls vmx_put_sbe32s
#define vmx_get_sbetls vmx_get_sbe32s
#endif*/
#endif

typedef void QEMUResetHandler(void *opaque);

void vmx_register_reset(QEMUResetHandler *func, void *opaque);
void vmx_unregister_reset(QEMUResetHandler *func, void *opaque);

//#ifdef NEED_CPU_H
#if TARGET_LONG_BITS == 64
#define VMSTATE_UINTTL_V(_f, _s, _v)                                  \
    VMSTATE_UINT64_V(_f, _s, _v)
#define VMSTATE_UINTTL_EQUAL_V(_f, _s, _v)                            \
    VMSTATE_UINT64_EQUAL_V(_f, _s, _v)
#define VMSTATE_UINTTL_ARRAY_V(_f, _s, _n, _v)                        \
    VMSTATE_UINT64_ARRAY_V(_f, _s, _n, _v)
#else
#define VMSTATE_UINTTL_V(_f, _s, _v)                                  \
    VMSTATE_UINT32_V(_f, _s, _v)
#define VMSTATE_UINTTL_EQUAL_V(_f, _s, _v)                            \
    VMSTATE_UINT32_EQUAL_V(_f, _s, _v)
#define VMSTATE_UINTTL_ARRAY_V(_f, _s, _n, _v)                        \
    VMSTATE_UINT32_ARRAY_V(_f, _s, _n, _v)
#endif
#define VMSTATE_UINTTL(_f, _s)                                        \
    VMSTATE_UINTTL_V(_f, _s, 0)
#define VMSTATE_UINTTL_EQUAL(_f, _s)                                  \
    VMSTATE_UINTTL_EQUAL_V(_f, _s, 0)
#define VMSTATE_UINTTL_ARRAY(_f, _s, _n)                              \
    VMSTATE_UINTTL_ARRAY_V(_f, _s, _n, 0)

//#endif

#endif

/*
 * Veery vmx support
 */

#ifndef __VEERTU_SYSTEM_H__
#define __VEERTU_SYSTEM_H__

#include "veertuemu.h"

static bool inline veertu_allow_irq0_override()
{
    return 1;
}
void veertu_reset_vcpu(X86CPU *cs);

#endif

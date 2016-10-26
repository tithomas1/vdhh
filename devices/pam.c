/*
 * QEMU Smram/pam logic implementation
 *
 * Copyright (c) 2006 Fabrice Bellard
 * Copyright (c) 2011 Isaku Yamahata <yamahata at valinux co jp>
 *                    VA Linux Systems Japan K.K.
 * Copyright (c) 2012 Jason Baron <jbaron@redhat.com>
 *
 * Split out from piix.c
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "typeinfo.h"
#include "sysemu.h"
#include "pam.h"

void smram_update(VeertuMemArea *smram_region, uint8_t smram,
                  uint8_t smm_enabled)
{
    bool smram_enabled;

    smram_enabled = ((smm_enabled && (smram & SMRAM_G_SMRAME)) ||
                        (smram & SMRAM_D_OPEN));
    mem_area_set_enable(smram_region, !smram_enabled, false);
}

void smram_set_smm(uint8_t *host_smm_enabled, int smm, uint8_t smram,
                   VeertuMemArea *smram_region)
{
    uint8_t smm_enabled = (smm != 0);
    if (*host_smm_enabled != smm_enabled) {
        *host_smm_enabled = smm_enabled;
        smram_update(smram_region, smram, *host_smm_enabled);
    }
}

void init_pam(DeviceState *dev, VeertuMemArea *ram_memory,
              VeertuMemArea *system_memory, VeertuMemArea *pci_address_space,
              PAMMemoryRegion *mem, uint32_t start, uint32_t size)
{
    int i;

    /* RAM */
    mem_area_init_alias(&mem->alias[3], "pam-ram", ram_memory,
                             start, size);
    /* ROM (XXX: not quite correct) */
    mem_area_init_alias(&mem->alias[1], "pam-rom", ram_memory,
                             start, size);
    mem_area_set_readonly(&mem->alias[1], true, false);

    /* XXX: should distinguish read/write cases */
    mem_area_init_alias(&mem->alias[0], "pam-pci", pci_address_space,
                             start, size);
    mem_area_init_alias(&mem->alias[2], "pam-pci", ram_memory,
                             start, size);

    for (i = 0; i < 4; ++i) {
        mem_area_set_enable(&mem->alias[i], false, false);
        mem_area_add_child_overlap(system_memory, start,
                                            &mem->alias[i], 1);
    }
    
    mem->current = 0;
}

void pam_update(PAMMemoryRegion *pam, int idx, uint8_t val)
{
    assert(0 <= idx && idx <= 12);

    mem_area_set_enable(&pam->alias[pam->current], false, false);
    pam->current = (val >> ((!(idx & 1)) * 4)) & PAM_ATTR_MASK;
    mem_area_set_enable(&pam->alias[pam->current], true, false);
}

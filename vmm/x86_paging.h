#ifdef PAGING_PAE

#undef PT_LEVEL_SHIFT
#define PT_LEVEL_SHIFT          9
#undef  PT_PAGE_SHIFT_LARGE
#define PT_PAGE_SHIFT_LARGE     21
#undef PTE_PA_MASK
#define PTE_PA_MASK             ((-1llu << PT_PAGE_SHIFT) & ((1llu << 52) - 1))

#undef gpte_t 
#define gpte_t                  uint64_t
#undef paging
#define paging(name)            pae_##name
#undef large_page_support
#define large_page_support(cpu) (1)
#undef CR3_MASK
#define CR3_MASK                (~0x1fllu)

#undef GPT_TOP_LEVEL
#define GPT_TOP_LEVEL           4

#undef PTE_LARGE_PA_MASK
#define PTE_LARGE_PA_MASK       ((-1llu << (PT_PAGE_SHIFT_LARGE)) & ((1llu << 52) - 1))

static inline bool paging(is_pae)()
{
    return true;
}

#elif defined(PAGING_LEGACY)

#undef PT_LEVEL_SHIFT
#undef PT_PAGE_SHIFT_LARGE
#undef PTE_PA_MASK
#undef gpte_t
#undef paging
#undef large_page_support
#undef CR3_MASK
#undef GPT_TOP_LEVEL

#define PT_LEVEL_SHIFT          10
#define PT_PAGE_SHIFT           12
#define PT_PAGE_SHIFT_LARGE     22
#define PTE_PA_MASK             (0xffffffffllu << (PT_PAGE_SHIFT))

#define gpte_t                  uint32_t
#define paging(name)            legacy_##name
#define large_page_support(cpu) is_pse_enabled(cpu)
#define CR3_MASK                (0xffffffff)
#define GPT_TOP_LEVEL           2

#undef PTE_LARGE_PA_MASK
#define PTE_LARGE_PA_MASK      ((-1llu << (PT_PAGE_SHIFT_LARGE)) & ((1llu << 32) - 1))

#define pse_pte_to_pa(pte) (((uint64_t) (pte & 0x1fe000) << 19) | (pte & 0xffc00000))

static inline bool paging(is_pae)()
{
    return false;
}

#else
#error "wrong configuration mmu"
#endif

#define PT_PAGE_SHIFT          12

#ifndef __X86_PAGING_H__
#define __X86_PAGING_H__

struct gpt_translation {
    addr_t  gva;
    addr_t gpa;
    int    err_code;
    uint64_t pte[5];
    bool write_access;
    bool user_access;
    bool exec_access;
};

#endif

static inline uint64_t paging(large_page_gpa)(struct gpt_translation *pt)
{
    VM_PANIC_ON(!pte_large(pt->pte[1]))
    /* 2Mb */
    if (paging(is_pae)())
        return (pt->pte[1] & PTE_LARGE_PA_MASK) | (pt->gva & 0x1fffff);

    /* 4Mb */
    return pse_pte_to_pa(pt->pte[1]) | (pt->gva & 0x3fffff);
}

static inline int paging(gpt_entry)(addr_t addr, int level)
{
    return (addr >> (PT_LEVEL_SHIFT * (level - 1) + PT_PAGE_SHIFT)) & ((1 << PT_LEVEL_SHIFT) - 1);
}

static int paging(pgt_top_level)(struct CPUState *cpu)
{
    if (!paging(is_pae)())
        return 2;
    if (x86_is_long_mode(cpu))
        return 4;

    return 3;
}

static bool paging(get_pt_entry)(struct CPUState *cpu, struct gpt_translation *pt, int level)
{
    int index;
    uint64_t pte = 0;
    addr_t gpa = pt->pte[level] & PTE_PA_MASK;
    if (level == 3 && !x86_is_long_mode(cpu))
        gpa = pt->pte[level];

    index = paging(gpt_entry)(pt->gva, level);
    address_space_rw(&address_space_memory, gpa + index * sizeof(gpte_t), (uint8_t *)&pte, sizeof(gpte_t), 0);

    pt->pte[level - 1] = pte;

    return true;
}

static bool paging(test_gpte)(struct CPUState *cpu, struct gpt_translation *pt, int level, bool *is_large)
{
    gpte_t pte = pt->pte[level];
    
    if (pt->write_access)
        pt->err_code |= MMU_PAGE_WT;
    if (pt->user_access)
        pt->err_code |= MMU_PAGE_US;
    if (pt->exec_access)
        pt->err_code |= MMU_PAGE_NX;
    
    if (!pte_present(pte))
        return false;

    if (paging(is_pae)() && !x86_is_long_mode(cpu) && 2 == level)
        goto exit;
    
    if (1 == level && pte_large(pte)) {
        pt->err_code |= MMU_PAGE_PT;
        *is_large = true;
    }
    if (!level)
        pt->err_code |= MMU_PAGE_PT;

    addr_t cr0 = rvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_CR0);

    if (cr0 & CR0_WP) {
        if (pt->write_access && !pte_writable(pte))
            return false;
    }

    if (pt->user_access && !pte_user(pte))
        return false;

    if (paging(is_pae)() && pt->exec_access && !pte_exec(pte))
        return false;

exit:
    return true;
}

static bool paging(walk_gpt)(struct CPUState *cpu, addr_t addr, int err_code, struct gpt_translation* pt)
{
    int top_level, level;
    bool is_large = false;
    addr_t cr3 = rvmcs(cpu->mac_vcpu_fd, VMCS_GUEST_CR3);

    memset(pt, 0, sizeof(*pt));
    top_level = paging(pgt_top_level)(cpu);

    pt->pte[top_level] = cr3 & CR3_MASK;
    pt->gva = addr;
    pt->user_access = !!(err_code & MMU_PAGE_US);
    pt->write_access = !!(err_code & MMU_PAGE_WT);
    pt->exec_access = !!(err_code & MMU_PAGE_NX);

    for (level = top_level; level > 0; level--) {
        paging(get_pt_entry)(cpu, pt, level);

        if (!paging(test_gpte)(cpu, pt, level - 1, &is_large))
            return false;

        if (is_large)
            break;
    }

    if (!is_large) {
        pt->gpa = (pt->pte[0] & PTE_PA_MASK) | (pt->gva & 0xfff);
    }
    else {
        pt->gpa = paging(large_page_gpa)(pt);
    }

    return true;
}

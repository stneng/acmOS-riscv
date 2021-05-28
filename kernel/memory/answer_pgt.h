void enable_paging() {
    // After initializing the page table, write to register SATP register for kernel registers.
    // Flush the TLB to invalidate the existing TLB Entries
    w_satp(MAKE_SATP(kernel_pagetable));
    flush_tlb();
}

// Return the address of the PTE in page table *pagetable*
// The Risc-v Sv48 scheme has four levels of page table.
// For VA:
//   47...63 zero
//   39...47 -- 9  bits of level-3 index
//   30...38 -- 9  bits of level-2 index
//   21...29 -- 9  bits of level-1 index
//   12...20 -- 9  bits of level-0 index
//   0...11  -- 12 bits of byte offset within the page
// Return the last-level page table entry.
static pte_t *pt_query(pagetable_t pagetable, vaddr_t va, int alloc) {
    if (va >= MAXVA) BUG_FMT("get va[0x%lx] >= MAXVA[0x%lx]", va, MAXVA);
    for (int i = 3; i > 0; i--) {
        pte_t *pte = &pagetable[PX(i, va)];
        if (*pte & PTE_V) {
            pagetable = (pagetable_t) PTE2PA(*pte);
        } else {
            if (alloc && (pagetable = mm_kalloc()) != NULL) {
                memset(pagetable, 0, BD_LEAF_SIZE);
                *pte = PA2PTE(pagetable) | PTE_V;
            } else {
                return NULL;
            }
        }
    }
    return &pagetable[PX(0, va)];
}

int pt_map_pages(pagetable_t pagetable, vaddr_t va, paddr_t pa, uint64 size, int perm) {
    uint64 st = PGROUNDDOWN(va), en = PGROUNDDOWN(va + size - 1);
    pte_t *pte;
    for (uint64 i = st; i <= en; i += PGSIZE, pa += PGSIZE) {
        if ((pte = pt_query(pagetable, i, 1)) == NULL) return -1;
        if (*pte & PTE_V) BUG("pt_map_pages: remap");
        *pte = PA2PTE(pa) | PTE_V | perm;
    }
    return 0;
}

paddr_t pt_query_address(pagetable_t pagetable, vaddr_t va) {
    pte_t *pte = pt_query(pagetable, va, 0);
    if (pte == NULL /*|| !(*pte & PTE_V)*/) BUG("pt_query_address: not found");
    return PTE2PA(*pte) | (va & 0xFFF);
}

int pt_unmap_addrs(pagetable_t pagetable, vaddr_t va) {
    pte_t *pte = pt_query(pagetable, va, 0);
    if (pte == NULL || !(*pte & PTE_V)) BUG("pt_unmap_addrs: not found");
    *pte = 0;
    return 0;
}

int pt_map_addrs(pagetable_t pagetable, vaddr_t va, paddr_t pa, int perm) {
    pte_t *pte = pt_query(pagetable, va, 1);
    if (pte == NULL) return -1;
    *pte = PA2PTE(pa) | PTE_V | perm;
    return 0;
}

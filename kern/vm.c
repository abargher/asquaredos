#include "vm.h"
#include "zalloc.h"
#include "resources.h"
#include <string.h>

/*
 * Table maps from page number (GROUP and INDEX bits) to PID.
 *
 * TODO: PIDs are constrained to 4 bits, so we can halve the size of this by
 *       packing two PIDs into each byte. This is just a space optimization, so
 *       I'll leave the added complexity for later.
 */
pid_t page_owner_table[1 << (GROUP_BITS + INDEX_BITS)];

/* TODO this table needs to be initialized to all PID_INVALID. */

/*
 * Write cache. Buffers write before evicting pages to flash once at capacity.
 */
page_t          write_cache[WRITE_CACHE_ENTRIES];
unsigned char   write_cache_bitmap[WRITE_CACHE_ENTRIES / 8];

/*
 * Only to be called from within the hardfault handler. Returns the address
 * which triggered the fault.
 */
static void *
get_faulting_address(void)
{
    /* TODO implement me. */
}

/*
 * User-accessible memory starts after kernel .bss section ends, and continues
 * until the end of last 32KB SRAM bank.
 */
extern char __bss_end__;
#define VM_START (&__bss_end__)
#define VM_END (SRAM_STRIPED_END)

/*
 * Walk the page table to find the PTE corresponding with the given address.
 * Returns NULL if no PTE has been allocated for that address.
 * 
 * TODO: should this function should be inlined for performance?
 */
pte_t *
address_to_pte(pte_group_table_t *pt, void *addr)
{
    pte_group_index_t group_index = pt->groups[GROUP(addr)];
    pte_group_t *pte_group = &pte_groups_base[group_index];
    if (pte_group == NULL) {
        /*
         * The PTE group will be NULL if this is the first time this subregion
         * has been accessed by the faulting process.
         */
        return NULL;
    }

    return &pte_group[INDEX(addr)];
}

/*
 * Finds a victim page to evict from the cache using LRU.
 */
pte_t *
vm_find_cache_victim(void)
{
    /* TODO. */
}

/*
 * Evicts the page mapped by the given PTE from the write cache, moving the
 * contents of the page into flash.
 */
void
vm_evict_cache_page(pte_t *pte)
{
    /* TODO. */
}

/*
 * Returns an unused page in the cache. This page is free if the cache is not
 * yet at capacity. Otherwise, the page will be stolen from a victim that gets
 * evicted to flash.
 */
cache_entry_index_t
vm_get_cache_page(void)
{
    /*
     * Scan the write cache bitmap to identify any free pages.
     */
    for (int i = 0; i < sizeof(write_cache_bitmap); i++) {
        /*
         * Invert so that free pages are 1s.
         */
        unsigned char bitmap_entry = ~write_cache_bitmap[i];

        /*
         * If any page is free, at least one bit will be set.
         */
        if (!bitmap_entry) {
            continue;
        }

        /*
         * There is at least one unset bit. Find which one it is, and claim it.
         *
         * TODO: some bit-hacking could probably speed this up (or even a
         *       dense lookup table with 256 entries --> 256 * 3 bits = 
         *       96 bytes to make that table).
         */
        for (int j = 0; j < 8; j++) {
            if (bitmap_entry & (1 << (j - 1))) {
                /*
                 * Claim the free page and return its index.
                 */
                write_cache_bitmap[i] |= (1 << (j - 1));
                return i * 8 + j;
            }
        }
    }

    pte_t *victim = vm_find_cache_victim();
    cache_entry_index_t entry = victim->cache.cache_index;
    vm_evict_cache_page(victim);

    return entry;
}

/*
 * Return a READ-ONLY pointer to the content pointed to by a PTE.
 */
page_t *
vm_get_page_contents(pte_t *pte)
{
    switch (pte->type) {
        case PTE_INVALID:
            return NULL;
        case PTE_SRAM:
            return SRAM_BASE + 256 * pte->sram.page_number;
        case PTE_CACHE:
            return write_cache + 256 * pte->cache.cache_index;
        case PTE_FLASH:
            return FLASH_SWAP_BASE + 256 * pte->flash.flash_index;
    }
}

/*
 * Do a bitwise comparison to check if the two pages are identical. We'll
 * eagerly return when we find anything that is not equal.
 * 
 * Returns true if identical, false if different.
 */
static bool
vm_check_pages_identical(page_t *page_a, page_t *page_b)
{
    long long *a = (long long *)page_a;
    long long *b = (long long *)page_b;

    /*
     * Compare 64 bits at a time...
     */
    for (int i = 0; i < PAGE_SIZE / sizeof(long long); i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }

    return true;
}

/*
 * Evict the given page from SRAM. Guarantees that the page is unoccupied upon
 * function return.
 * 
 * This also checks if the page has actually been dirtied to determine whether
 * anything needs to be copied out to the write cache, or whether the page can
 * be silently dropped.
 */
void
vm_evict_sram_page(page_t *page)
{
    /*
     * Find who currently owns this page. If no-one does, then we can treat it
     * as already having been evicted.
     */
    pid_t owner = page_owner_table[PAGE_NUMBER(page)];
    if (owner == PID_INVALID) {
        return;
    }

    /*
     * Otherwise, we'll get the owner's page table and figure out what to do
     * with the page, depending on whether or not it's dirty.
     */
    pte_group_table_t *pt = &pte_group_tables_base[owner];
    pte_t *pte = address_to_pte(pt, page);
    if (pte == NULL || pte->type == PTE_INVALID) {
        /*
         * This shouldn't happen, but it should be equivalent to the earlier
         * case of no one owning the page.
         */
        return;
    }

    /*
     * If this page is backed by the write cache or flash then we can check it
     * is clean; if it's clean, we can silently drop it.
     */
    if (pte->type > PTE_SRAM) {
        page_t *backing_page = vm_get_page_contents(pte);
        if (vm_check_pages_identical(page, backing_page)) {
            return;
        }
    }

    /*
     * At this point, the page cannot be silently dropped. It needs to be
     * evicted into the write cache, and if the backing PTE was a flash PTE
     * then we need to mark that flash sector as invalid in our flash bitmap.
     */
    
    /*
     * If the page is backed by a write cache entry, we can re-use that entry.
     */
    if (pte->type == PTE_CACHE) {
        memcpy(vm_get_page_contents(pte), page, PAGE_SIZE);

        /* TODO: update whatever LRU mechanism we'll have in the write cache. */

        return;
    }

    /*
     * At this point we need to procure an entry in the write cache and copy the
     * contents of the page we're evicting from SRAM into it.
     */

    /* TODO. */
}



/*
 * Look up each PTE associated with the given MPU subregion and read in the data
 * it stores. If this region is uninitialized for the given page table, a PTE
 * group will be allocated and configured to map it, and the contents of the
 * subregion in SRAM will be zeroed.
 * 
 * Guarantees that the process' page table is configured, and the contents of
 * the subregion in SRAM are correct upon function return.
 * 
 * TODO: implement a simple optimization to prevent us swapping out zeroes.
 */
void
vm_get_subregion(pte_group_table_t *pt, void *subregion)
{
    /* TODO. */
}


/*
 * This fault handler will replace the hardfault handler, and will fall through
 * to that handler if the faulting address is not in a valid region of SRAM
 * (at least for now, until we determine a reasonable way to kill a process.)
 */
void
vm_fault_handler(void)
{
    void *addr = get_faulting_address();
    if (addr < VM_START || addr >= VM_END) {
        /* TODO: call original hardfault handler or kill the process... */
        exit(-1);
    }

    /*
     * The faulting process needs to be given the 4KB MPU sub-region that this
     * address falls into. This means evicting any valid pages that are
     * currently in the sub-region, and reading in the faulting process's data.
     * 
     * We need to consider every page in the MPU subregion because MPU
     * permissions can only be set at MPU subregion granularity, and so we
     * would be unable to protect and identify accesses to the other pages
     * otherwise.
     */

    /*
     * Iterate through each page contained in the MPU subregion and evict it.
     */
    void *subregion_base = MPU_SUBREGION_BASE(addr);
    for (unsigned offset = 0; offset < MPU_SUBREGION_SIZE; offset += PAGE_SIZE) {
        page_t *page = subregion_base + offset;
        vm_evict_sram_page(page); /* TODO. */
    }

    /*
     * Read in all the data mapped by the process' page table for this subregion
     * or otherwise allocate and initialize that portion of the page table if
     * it does not exist.
     */
    vm_get_subregion(&pcb_active->page_table, subregion_base); /* TODO. */
    mpu_enable_subregion(subregion_base); /* TODO. */

    /*
     * Re-try the instruction that triggered the fault (TODO).
     */
    /* TODO. */
}

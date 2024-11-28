#include <string.h>
#include <stdio.h>

#include <hardware/flash.h>

#include "vm.h"
#include "zalloc.h"
#include "resources.h"
#include "third-party/m0FaultDispatch.h"

/*
 * Table maps from page number (GROUP and INDEX bits) to PID.
 *
 * TODO: PIDs are constrained to 4 bits, so we can halve the size of this by
 *       packing two PIDs into each byte. This is just a space optimization, so
 *       I'll leave the added complexity for later.
 */
pid_t sram_page_owner_table[1 << (GROUP_BITS + INDEX_BITS)];

/*
 * Table maps from cache_index_t to the PID of that entry's owner.
 *
 * TODO: Same space-saving optimization as described for the SRAM owner table.
 */
pid_t write_cache_entry_owner_table[WRITE_CACHE_NUM_ENTRIES];

/* TODO these tables need to be initialized to all PID_INVALID. */

/*
 * Bitmap tracking which pages are occupied (1) and free (0) in the region of
 * flash that we evict write cache entries to. The sector bitmap tracks which
 * pages have never been erased (1) and have been erased (0).
 */
unsigned char flash_page_bitmap[FLASH_SWAP_NUM_PAGES / 8];
unsigned char flash_sector_bitmap[FLASH_SWAP_NUM_SECTORS / 8];

/*
 * Write cache. Buffers write before evicting pages to flash once at capacity.
 */
page_t          write_cache[WRITE_CACHE_NUM_ENTRIES];
unsigned char   write_cache_bitmap[WRITE_CACHE_NUM_ENTRIES / 8];

#define BITMAP_OPERATION_FAILED (-1)

/*
 * Scans up to an entire bitmap, starting from the provided index, to find the
 * index of a zero. Updates the zero to a one, and returns the index.
 * 
 * Returns BITMAP_OPERATION_FAILED if no zero could be found.
 */
unsigned int
bitmap_find_and_set_first_zero(
    unsigned char  *bitmap,
    unsigned int    size_bits,
    unsigned int    start_at)
{
    /*
     * Scan the bitmap to identify any 0s.
     */
    for (int i = 0; i < size_bits; i++) {
        unsigned int index = (start_at + i) % size_bits;

        /*
         * Invert so we can quickly identify if all bits are set.
         */
        unsigned char bitmap_entry = ~bitmap[index];

        /*
         * If no bits are set in the inverted entry then there were no zeros in
         * the original, uninverted entry.
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
            if (bitmap_entry & (1 << j)) {
                /*
                 * Claim the free page and return its index.
                 */
                bitmap[index] |= (1 << j);
                return 8 * index + j;
            }
        }
    }

    return BITMAP_OPERATION_FAILED;
}

/*
 * Walk the page table to find the PTE corresponding with the given address.
 * Returns NULL if no PTE has been allocated for that address.
 * 
 * TODO: should this function should be inlined for performance?
 */
pte_t *
address_to_pte(
    pte_group_table_t  *pt,
    void               *addr)
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

    return &pte_group->ptes[INDEX(addr)];
}

/*
 * Return a READ-ONLY pointer to the content pointed to by a PTE.
 */
static inline page_t *
vm_get_page_contents(pte_t *pte)
{
    switch (pte->type) {
        case PTE_INVALID:
            return NULL;
        case PTE_SRAM:
            return (page_t *)SRAM_PAGE(pte->sram.page_number);
        case PTE_CACHE:
            return (page_t *)CACHE_PAGE(pte->cache.cache_index);
        case PTE_FLASH:
            return (page_t *)FLASH_PAGE(pte->flash.flash_index);
        default:
            panic("PTE has unknown type; should not be possible");
    }
}

/*
 * Acquires a free (already erased and unwritten) flash page from the swap
 * region. This function will panic if no sector can be found.
 */
flash_index_t
vm_procure_flash_page(void)
{
    static unsigned int page_bitmap_start_at = 0;
    static unsigned int sector_bitmap_start_at = 0;

    /*
     * Try to find a free page.
     */
    unsigned int page_index = bitmap_find_and_set_first_zero(
        flash_page_bitmap,
        FLASH_SWAP_NUM_PAGES,
        page_bitmap_start_at);
    if (page_index != BITMAP_OPERATION_FAILED) {
        page_bitmap_start_at = page_index + 1;
        return FLASH_INDEX_FROM_SWAP_INDEX(page_index);
    }
    
    /*
     * We couldn't find a free sector, so let's check if there's a page we've
     * never erased, in which case we erase it and mark all of its pages as
     * free.
     */
    unsigned int sector_index = bitmap_find_and_set_first_zero(
        flash_sector_bitmap,
        FLASH_SWAP_NUM_SECTORS,
        sector_bitmap_start_at);
    if (sector_index != BITMAP_OPERATION_FAILED) {
        flash_range_erase(
            FLASH_SWAP_BASE_FLASH_OFS + sector_index * FLASH_SECTOR_SIZE,
            FLASH_SECTOR_SIZE);
#ifdef FLASH_DEBUG
        /*
         * Ensure that we haven't worn out our flash (that the erase actually
         * succeeded in resetting all bits to 1).
         */
        unsigned int n_failures = 0;
        for (int i = 0; i < FLASH_SECTOR_SIZE; i++) {
            if (*(FLASH_SWAP_BASE + sector_index * FLASH_SECTOR_SIZE + i) != 0xFF) {
                n_failures++;
            }
        }
        if (n_failures > 0) {
            panic(
                "flash erase failed; detected %u failed bytes for sector index %u (mapped at %p)",
                n_failures,
                sector_index,
                (void *)(FLASH_SWAP_BASE + sector_index * FLASH_SECTOR_SIZE));
        }
#endif /* FLASH_DEBUG */
        /*
         * Update the page bitmap to indicate that we've erased this sector, and
         * that we'll be claiming its first page.
         */
        page_index = sector_index * (FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE);
        flash_page_bitmap[page_index + 0] = 0x01;   /* All unoccupied except first page. */
        flash_page_bitmap[page_index + 1] = 0x00;

        /*
         * TODO: I (with fresh eyes)--or someone else--should sanity-check that
         * the math makes sense to be the first page in the newly erased sector.
         */
        return FLASH_INDEX_FROM_SWAP_INDEX(page_index);
    }

    /*
     * We failed to find a free page, and there were no unerased sectors. Until
     * we implement some sort of garbage collection we'll need to panic here,
     * since there's no currently implemented way to acquire a page.
     */
    panic("failed to procure a flash page; TODO: implement garbage collection");

    __builtin_unreachable;
    return 0;
}

/*
 * Evicts the page mapped by the given PTE from the write cache, moving the
 * contents of the page into flash.
 */
void
vm_evict_cache_entry(pte_t *pte)
{
    /*
     * Get an unused sector (same size as a page) in flash. Note this will panic
     * if it fails to procure a page, so no error checking is required.
     */
    flash_index_t flash_index = vm_procure_flash_page();
    
    /*
     * Write the contents of the cache entry to the flash page.
     */
    flash_range_program(
        FLASH_OFFSET(flash_index),
        (uint8_t *)vm_get_page_contents(pte),
        PAGE_SIZE);
    
    /*
     * Update the PTE to point to flash.
     */
    pte->type = PTE_FLASH;
    pte->flash.flash_index = flash_index;

    /*
     * This cache entry can now be safely overwritten.
     */
}

/*
 * Finds a victim page to evict from the cache using LRU.
 */
pte_t *
vm_find_cache_victim(void)
{
    /*
     * In order to ensure fairness, we need to always run the eviction clock
     * hand starting from the last place we found a victim.
     */
    static cache_index_t index = 0;

    /*
     * Keep looping over the cache entries until we find one that 
     */
    while (1) {
        /*
         * Not really necessary since index is only a char and can't exceed 255,
         * but it doesn't really hurt.
         */
        index &= 0xFF;

        /*
         * Meta TODO for performance: it seems somewhat inefficient to have to
         * re-do this lookup every single time... we could change the lookup
         * table to instead give the index of the PTE that owns the entry,
         * instead of the PTE group table that owns the entry.
         */

        /*
         * Find who owns the 
         */
        pid_t owner = write_cache_entry_owner_table[index];
        pte_group_table_t *pt = &pte_group_tables_base[owner];
        if (owner == PID_INVALID) {
            /*
             * If no-one owns this page (this should not happen, since we should
             * have already scanned the bitmap for a free page) then we'll just
             * use this page and make sure it's marked in the bitmap.
             */
            write_cache_bitmap[index / 8] |= (1 << index % 8);
            return address_to_pte(pt, CACHE_PAGE(index++));
        }
        pte_t *pte = address_to_pte(pt, CACHE_PAGE(index));

        if (pte->cache.dirty == 0) {
            return pte;
        }

        /*
         * This entry is not yet evictable. Age the entry and try the next one.
         */
        pte->cache.dirty--;
        index++;
    }
}

/*
 * Returns an unused page in the cache. This page is free if the cache is not
 * yet at capacity. Otherwise, the page will be stolen from a victim that gets
 * evicted to flash.
 */
cache_index_t
vm_procure_cache_entry(void)
{
    static unsigned int bitmap_start_at = 0;

    /*
     * Try to find an unused entry, if one exists.
     */
    unsigned int index = bitmap_find_and_set_first_zero(
        write_cache_bitmap,
        sizeof(write_cache_bitmap) * 8,
        bitmap_start_at);
    if (index != BITMAP_OPERATION_FAILED) {
        bitmap_start_at = index + 1;
        return (cache_index_t)index;
    }

    /*
     * We couldn't find an unused entry, so we need to evict one.
     */
    pte_t *victim = vm_find_cache_victim();
    cache_index_t entry = victim->cache.cache_index;
    vm_evict_cache_entry(victim);

    return entry;
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
    pid_t owner = sram_page_owner_table[PAGE_NUMBER(page)];
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
         * Equivalent to the earlier case of no one owning the page.
         */
        return;
    }

    /*
     * If this page is backed by the write cache or flash then we can check it
     * is clean; if it's clean, we can silently drop it.
     */
    if (pte->type > PTE_SRAM) {
        page_t *backing_page = vm_get_page_contents(pte);
        if (memcmp(page, backing_page, PAGE_SIZE) == 0) {
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
        pte->cache.dirty++;

        return;
    }

    /*
     * At this point we need to procure an entry in the write cache and copy the
     * contents of the page we're evicting from SRAM into it.
     */

    /*
     * Procure an entry in the write cache and update this PTE to point to it.
     */
    cache_index_t entry = vm_procure_cache_entry();
    write_cache_entry_owner_table[entry] = owner;
    memcpy(CACHE_PAGE(entry), page, PAGE_SIZE);
    pte->type = PTE_CACHE;
    pte->cache.cache_index = entry;

    /*
     * My goal is to evict pages which are least frequently dirtied, since the
     * purpose of the write cache is to reduce the number of writes/erases we
     * need to make to flash, and these pages seem likely to be dirtied less in
     * the near-future.
     * 
     * I'm not 100% sure what we should set the dirty counter to initially. We
     * should probably protect newly cached pages from being immediately thrown
     * out if we initialize the value to 1. (i.e., is a newly cached entry worth
     * more than an entry that's already at a low "frequency" of access?)
     */
    pte->cache.dirty = 2;

    /*
     * The SRAM page can now be safely overwritten.
     */
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
 *          --> perhaps a special bit set for the flash pte type or something?
 */
void
vm_read_in_subregion(
    pte_group_table_t  *pt,
    void               *subregion)
{
    /*
     * Check if PTEs exist to map this subregion. If not, we need to allocate
     * and configure them, since this is the first time this processes will have
     * accessed an address in this MPU subregion.
     */
    if (pt->groups[GROUP(subregion)] == PTE_GROUP_INVALID) {
        /*
         * Allocate a new PTE group.
         */
        pte_group_t *pte_group = zalloc(KZONE_PTE_GROUP);
        if (pte_group == NULL) {
            panic("failed to allocate PTE group; zone exhausted");
        }

        /*
         * Initialize the PTEs as pointing to the page they represent in SRAM,
         * and register the PTE group.
         */
        for (int i = 0; i < GROUP_SIZE; i++) {
            pte_t *pte = &pte_group->ptes[i];
            pte->type = PTE_SRAM;
            pte->sram.page_number = PAGE_NUMBER(subregion) + i;
        }
        pt->groups[GROUP(subregion)] = GROUP_INDEX(pte_group);

        /*
         * Is the memset necessary? Not sure what the performance hit is, but it
         * means memory contents won't be leaked between processes.
         */
        memset(subregion, 0, MPU_SUBREGION_SIZE);

        return;
    }

    /*
     * PTEs already exist to map these pages, which means we can just copy in
     * the content pointed to by those PTEs.
     */
    for (page_t *page = subregion;
         (uintptr_t)page < (uintptr_t)(subregion + MPU_REGION_SIZE);
         page++) {
        pte_t *pte = address_to_pte(pt, page);
        memcpy(page, vm_get_page_contents(pte), PAGE_SIZE);
    }
}

void
mpu_enable_subregion(void *subregion)
{
    /* TODO: define me. */
}

/*
 * Create a non-swap flash PTE mapping from the source page (in flash) to the
 * destination page (in SRAM). Caller assumes the responsibility of making sure
 * the source page is sane.
 * 
 * Note that pages back by flash are never directtly written back to the source
 * page in flash when dirtied; pages mapped from outside of the swap region are
 * thus treated as read-only by the VM system.
 */
void
vm_map_generic_flash_page(
    pte_group_table_t  *pt,
    page_t             *src,
    page_t             *dst)
{
    /*
     * If a PTE doesn't already exist for this page, allocate it right now.
     */
    pte_group_index_t index = GROUP(dst);
    if (pt->groups[index] == PTE_GROUP_INVALID) {
        pte_group_t *group = zalloc(KZONE_PTE_GROUP);
        if (group == NULL) {
            panic("failed to allocate pte_group_t");
        }
        pt->groups[index] = GROUP_INDEX(group);

        /*
         * Note the zalloc will zero the PTE group, marking them as invalid.
         */
    }

    /*
     * Point this PTE at the source page.
     */
    pte_t *pte = address_to_pte(pt, dst);
    pte->type = PTE_FLASH;
    pte->flash.flash_index = FLASH_INDEX(src);
}

/*
 * Initialize all global state used by the VM system.
 */
void
vm_init(void)
{
    /* TODO. */
}

/*
 * This fault handler will replace the hardfault handler, and will fall through
 * to that handler if the faulting address is not in a valid region of SRAM
 * (at least for now, until we determine a reasonable way to kill a process.)
 */
__attribute__((noreturn))
void
vm_fault_handler(void *addr)
{
    if (addr < VM_START || addr >= VM_END) {
        /* TODO: call original hardfault handler or kill the process... */
        panic("faulting address out of range; TODO implement handling");
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
     * Iterate through each page contained in the MPU subregion and evict it,
     * unless it's already owned by the active process.
     */
    pid_t pid_active = PID_FROM_GROUP_TABLE(pcb_active->page_table);
    void *subregion_base = MPU_SUBREGION_BASE(addr);
    for (unsigned offset = 0; offset < MPU_SUBREGION_SIZE; offset += PAGE_SIZE) {
        page_t *page = subregion_base + offset;
        pid_t *pid = &sram_page_owner_table[PAGE_NUMBER(page)];
        if (*pid != pid_active) {
            vm_evict_sram_page(page);
            *pid = pid_active;
        }
    }

    /*
     * Read in all the data mapped by the process' page table for this subregion
     * or otherwise allocate and initialize that portion of the page table if
     * it does not exist.
     */
    vm_read_in_subregion(pcb_active->page_table, subregion_base);
    mpu_enable_subregion(subregion_base); /* TODO. */

    /*
     * Re-try the instruction that triggered the fault. Note that this will
     * fail badly if the fault did not occur while it was in thread mode, and
     * while it was using the program stack pointer. This should never be the
     * case if this code is reached, however, since at this point the fault must
     * have been a read or write fault in the VM-managed range of SRAM, which
     * would not occur while executing in handler mode, and we never use the MSP
     * while in thread mode.
     */
    asm volatile (
        ".thumb                     \n\t"
        ".align 4                   \n\t"
        "mrs    r0, psp             \n\t"
        "ldr    r1, [r0, #24]       \n\t"   /* Load saved PC from the program stack. */
        "sub    r1, #2              \n\t"   /* Decrement PC to point at faulting instruction. */
        "str    r1, [r0, #24]       \n\t"   /* Save modified PC back to the program stack. */
        "pop    {r0, pc}            \n\t"   /* Return from the exception. */
        :
        :
        : "memory", "r0", "r1");

    __builtin_unreachable();
}

void
fault_handler_with_exception_frame(
    struct CortexExcFrame *exc,
    uint32_t cause,
    uint32_t extra_data,
    struct CortexPushedRegs *hi_regs)
{
    /*
     * The VM fault handler is 
     */
    if (cause == EXC_m0_CAUSE_MEM_READ_ACCESS_FAIL ||
        cause == EXC_m0_CAUSE_MEM_WRITE_ACCESS_FAIL) {

        vm_fault_handler((void *)extra_data);   /* extra_data is the faulting address. */

        /*
         * At this point vm_fault_handler has returned, indicating that this
         * fault cannot be handled by the VM system, and should be treated as
         * any other unhandled fault.
         * 
         * Handled faults will result in vm_fault_handler not returning; the
         * handler will return from the exception state, loading the PC directly
         * in order to re-execute the faulting instruction.
         */
    }
    
    /*
     * At this point the exception is to be considered unhandled. Until a means
     * of terminating a process is implemented, the system will instead dump
     * debugging information and halt.
     * 
     * The logging code below is taken from the example handler implementation
     * provided by the m0FaultDispatch project.
     */

    m0_fault_dump_registers_and_halt(exc, cause, extra_data, hi_regs);

    __builtin_unreachable();
}
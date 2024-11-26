#include "vm.h"

/*
 * Table maps from page number (GROUP and INDEX bits) to PID.
 *
 * TODO: PIDs are constrained to 4 bits, so we can halve the size of this by
 *       packing two PIDs into each byte. This is just a space optimization, so
 *       I'll leave the added complexity for later.
 */
pid_t page_owner_table[1 << (GROUP_BITS + INDEX_BITS)];

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
     * Iterate through each page contained in the MPU subregion and evict it,
     * and either allocate a new PTE to track the faulting process' new page
     * contents, or read in the data pointed to by its existing PTE in the
     * faulting process' page table.
     */
    void *subregion_base = MPU_SUBREGION_BASE(addr);
    for (unsigned offset = 0; offset < MPU_SUBREGION_SIZE; offset += PAGE_SIZE) {
        page_t *page = subregion_base + offset;
        vm_evict_sram_page(page);
        vm_get_page_data(page, )
    }

    /*
     * Check who owns the PTE that's already there.
     */
    pid_t owner = page_owner_table[PAGE_NUMBER(addr)];
    if (owner == PID_INVALID) {
        
    }
}

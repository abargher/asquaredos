#define KERNEL

#include "boot.h"

/* TODO use the real value from the SDK. */
#define KB 1024
#define SRAM_SIZE 256 * KB
#define SRAM_START 0x20000000

/* TODO find good value. */
#define STACK_SIZE 4 * KB

#define NULL (void *)0

/*
 * Layout of our shared kernel state, defined by the linker script.
 */
extern char __shared_variables_start;
extern char __shared_variables_end;

/*
 * TODO: find out if the RAM section of the .elf can be modified at runtime
 *       without breaking things, or if there's a compilation option to allow
 *       this sort of behavior.
 * 
 *       the problem is that we think by default a binary is dependent on a
 *       known fixed base address, and that's not amenable to us dynamically
 *       allocating text/data/bss.
 */
inline
int
create_system_resources(
    uint32_t text_size,
    uint32_t data_size,
    uint32_t bss_size,
    void *flash_elf_text_section,
    void *flash_elf_const_section,
    void *flash_elf_bss_section)
{
    /* fun note for later: this will point to self, do we need allocated list head any more? */
    pcb_t *pcb = kalloc(sizeof(pcb_t), NULL);
    assert(pcb);    /* TODO implement assert that panics if false. */

    /*
     * This PCB itself is the first thing we've allocated, and becomes the head
     * of the allocated list.
     */
    LL_INSERT(pcb->allocated, pcb - sizeof(heap_region_t)); /* TODO implement LL macros. */

    /*
     * Make this PCB schedulable.
     */
    if (pcb_active == NULL) {
        pcb_active = pcb;
    }
    LL_INSERT_CIRCULAR(pcb_active->sched_next, pcb);    /* TODO implement macro, and should probably insert onto the end of the list..? */

    /*
     * TODO: Allocate sizeof(data + bss) then copy const into data (or something to that effect, todo figure this out).
     */

    /*
     * Initialize special-purpose registers.
     */
    pcb->registers.sp = kalloc(STACK_SIZE, pcb);
    assert(pcb->registers.sp);      /* TODO implement assert that panics if false. */
    pcb->registers.pc = flash_elf_text_section;



}

/*
 * Third stage bootloader.
 */
__attribute__((noreturn))
int
main(void)
{
    /*
     * Initialize the allocator.
     */
    heap_start = (void *)&__shared_variables_end;
    heap_free_list = (heap_region_t *)heap_start;
    heap_free_list->next = NULL;
    heap_free_list->prev = NULL;
    heap_free_list->size = SRAM_SIZE - (uint32_t)(&__shared_variables_end - SRAM_START);

    /* Get programs to run, and their sizes..? */
    


}

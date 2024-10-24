#define KERNEL

#include "boot.h"
#include "utils/list.h"

/* TODO use the real value from the SDK. */
#define KB 1024
#define SRAM_SIZE 256 * (KB)
#define SRAM_START 0x20000000

/* TODO find good value. */
#define STACK_SIZE 4 * (KB)

/* Byte granularity at which the MPU can protect a region of memory. */
#define MPU_REGION_GRANULARITY_BITS 8
#define MPU_REGION_GRANULARITY 1 << (MPU_REGION_GRANULARITY_BITS)

#define NULL (void *)0

/*
 * Layout of our private kernel state, defined by the linker script.
 */
extern char __kernel_private_state_start;
extern char __kernel_private_state_end;

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
void
create_system_resources(
    uint32_t text_size,
    uint32_t data_size,
    uint32_t bss_size,
    uint32_t got_size,
    void *flash_elf_text_section,
    void *flash_elf_const_section,
    void *flash_elf_bss_section,
    void *flash_elf_got_section)
{
    pcb_t *pcb = (pcb_t *)zalloc(KZONE_PCB);
    assert(pcb != NULL);

    /*
     * The PCB starts with no allocated memory.
     */
    pcb->allocated = NULL;

    /*
     * Make this PCB schedulable.
     */
    DLL_PUSH(ready_queue, pcb, next, prev);

    /*
     * TODO: Allocate sizeof(data + bss) then copy const into data (or something to that effect, todo figure this out).
     */


    /*
     * Initialize special-purpose registers.
     *
     * TODO: one of us needs to come through and find the right initial values
     *       based on what the datasheet says.
     */
    pcb->registers.sp = palloc(STACK_SIZE, pcb);
    assert(pcb->registers.sp);      /* TODO implement assert that panics if false. */
    pcb->registers.pc = flash_elf_text_section;
    pcb->registers.control = CONTROL_UNPRIVILEGED;  /* TODO get from SDK. */
    pcb->registers.primask = PRIORITY_MASK_DEFAULT; /* TODO decide on a default. */
    pcb->registers.psr = 0;                         /* TODO deicde on initial value. */
    pcb->registers.lr = 0;                          /* TODO decide on initial value. */

    /*
     * At this point this process should be ready-to-run when the scheduler
     * selects this process control block to run.
     */
}

/*
 * Third stage bootloader.
 */
__attribute__((noreturn))
int
main(void)
{
    /*
     * Initialize the zone allocator.
     */
    zinit();

    /* TODO: make a "phony" kernel PCB for as the active PCB to bootstrap scheduler. */

    /*
     * The heap will begin aligned with the first MPU-protectable address after
     * the kernel private state section ends. The heaps begins as a singleton
     * free element.
     */
    heap_start = &__kernel_private_state_end + ((MPU_REGION_GRANULARITY) -
        (uint32_t)&__kernel_private_state_end % (MPU_REGION_GRANULARITY));
    heap_free_list = (heap_region_t *)heap_start;
    heap_free_list->next = NULL;
    heap_free_list->prev = NULL;
    heap_free_list->size = (SRAM_SIZE) - (uint32_t)(&__kernel_private_state_end - (SRAM_START));

    /* Get programs to run, and their sizes..? */

}

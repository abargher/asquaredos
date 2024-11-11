#define KERNEL

#include <string.h>
#include "boot.h"
#include "utils/list.h"
#include "utils/panic.h"

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
    void       *bin_start,  /* Start of the binary in FLASH. */
    uint32_t    bin_size,   /* Size of the binary in FLASH. */
    uint32_t    bss_size)   /* Size of uninitialized data not in .bin file. */
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
     * Allocate a stack.
     */
    void *stack = palloc(STACK_SIZE, pcb);
    assert(stack != NULL);
    pcb->saved_sp = (uint32_t)stack + STACK_SIZE;   /* Start at TOP of the stack. */

    /*
     * Allocate space for the program's execution state ("binary") in memory.
     */
    void *bin = palloc(bin_size + bss_size, pcb);
    assert(bin != NULL);
    memcpy(bin, bin_start, bin_size);

    /*
     * Resume execution at the beginning of the binary.
     */
    stack_registers_t *stack_registers = (stack_registers_t *)stack;
    stack_registers->lr = bin;

    /*
     * Align stack pointer correctly pop our initial saved registers.
     */
    pcb->saved_sp -= sizeof(stack_registers_t);

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

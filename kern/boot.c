#define KERNEL

#include <string.h>
#include "boot.h"
#include "utils/list.h"
#include "utils/panic.h"
#include "palloc.h"
#include "zalloc.h"
#include "context_switch.h"
#include "pico/stdlib.h"
#include "hardware/exception.h"

/* TODO use the real value from the SDK. */
#define KB 1024
#define SRAM_SIZE 256 * (KB)
#define SRAM_START 0x20000000

/* TODO find good value. */
#define STACK_SIZE 4 * (KB)

/* Byte granularity at which the MPU can protect a region of memory. */
#define MPU_REGION_GRANULARITY_BITS 8
#define MPU_REGION_GRANULARITY 1 << (MPU_REGION_GRANULARITY_BITS)

/*
 * Layout of our private kernel state, defined by the linker script.
 */
extern char __data_start__;
extern char __data_end__;

/*
 * TODO: find out if the RAM section of the .elf can be modified at runtime
 *       without breaking things, or if there's a compilation option to allow
 *       this sort of behavior.
 *
 *       the problem is that we think by default a binary is dependent on a
 *       known fixed base address, and that's not amenable to us dynamically
 *       allocating text/data/bss.
 */
// inline
void
create_system_resources(
    void       *load_from,  /* Start of the binary in FLASH. */
    void       *load_to,    /* Start of the binary in SRAM. */
    uint32_t    load_size)  /* Number of bytes to copy from flash. */
{
    /* TODO do we need extra space allocated for .bss (not in .bin) in SRAM? */

    pcb_t *pcb = (pcb_t *)zalloc(KZONE_PCB);
    assert(pcb != NULL);

    /*
     * The PCB starts with no allocated memory.
     */
    pcb->allocated = NULL;

    /*
     * Allocate a stack.
     */
    void *stack = palloc(STACK_SIZE, pcb, PALLOC_FLAGS_ANYWHERE, NULL);
    assert(stack != NULL);
    pcb->saved_sp = (uint32_t)stack + STACK_SIZE;   /* Start at TOP of the stack. */

    /*
     * Allocate space for the program's execution state ("binary") in memory.
     */
    void *out = palloc(load_size, pcb, PALLOC_FLAGS_FIXED, load_to);
    assert(out == load_to);
    memcpy(load_to, load_from, load_size);

    /*
     * Resume execution at the beginning of the binary.
     */
    stack_registers_t *stack_registers = (stack_registers_t *)stack;
    stack_registers->lr = (register_t)load_to;

    /*
     * Align stack pointer correctly pop our initial saved registers.
     */
    pcb->saved_sp -= sizeof(stack_registers_t);

    /*
     * Make this PCB schedulable.
     */
    DLL_PUSH(ready_queue, pcb, next, prev);

    /*
     * At this point this process should be ready-to-run when the scheduler
     * selects this process control block to run.
     */
}

/*
 * Third stage bootloader.
 */
// __attribute__((noreturn))
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
    heap_start = &__data_end__ + ((MPU_REGION_GRANULARITY) -
        (uint32_t)&__data_end__ % (MPU_REGION_GRANULARITY));
    heap_free_list = (heap_region_t *)heap_start;
    heap_free_list->next = NULL;
    heap_free_list->prev = NULL;
    heap_free_list->size = (SRAM_SIZE) - (uint32_t)(&__data_end__ - (SRAM_START));

    /* Get programs to run, and their sizes..? */

    create_system_resources((void *)0x10010000, (void *)0x20010000, (64 * 1024));
    create_system_resources((void *)0x10020000, (void *)0x20020000, (64 * 1024));

    exception_set_exclusive_handler(SVCALL_EXCEPTION, schedule_handler);
    asm("mrs r0, msp");
    asm("msr psp, r0");
    asm("svc #0");

    // p/x *(stack_registers_t * )($r2 - sizeof(stack_registers_t))
    // 
    // schedule_handler();
}

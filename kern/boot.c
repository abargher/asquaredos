#define KERNEL

#include <string.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/exception.h"
#include "hardware/structs/systick.h"
#include "hardware/structs/mpu.h"

#include "boot.h"
#include "utils/list.h"
#include "utils/panic.h"
#include "palloc.h"
#include "zalloc.h"
#include "context_switch.h"
#include "scheduler.h"
#include "vm.h"


#define SRAM_SIZE 256 * (KB)
#define SRAM_START 0x20000000

#define STACK_SIZE 4 * (KB)

/* Byte granularity at which the MPU can protect a region of memory. */
#define MPU_REGION_GRANULARITY_BITS 8
#define MPU_REGION_GRANULARITY 1 << (MPU_REGION_GRANULARITY_BITS)

/*
 * Layout of our private kernel state, defined by the linker script.
 */
extern char __data_start__;
extern char __bss_end__;

void
create_system_resources(
    void       *load_from,  /* Start of the binary in FLASH. */
    void       *load_to,    /* Start of the binary in SRAM. */
    void       *exec_from,  /* Entrypoint of binary in SRAM. */
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
    pcb->saved_sp = ((uint32_t)stack + STACK_SIZE) & ~(0b111);   /* Start at TOP of the stack. */

    /*
     * Allocate space for the program's execution state ("binary") in memory.
     */
    void *out = palloc(load_size, pcb, PALLOC_FLAGS_FIXED, load_to);
    assert(out == load_to);
    memcpy(load_to, load_from, load_size);

    /*
     * Align stack pointer correctly to pop our initial saved registers.
     */
    pcb->saved_sp -= sizeof(stack_registers_t);

    /*
     * Resume execution at the beginning of the binary.
     */

    stack_registers_t *stack_registers = (stack_registers_t *)pcb->saved_sp;

    /* Set some stack register values for easy recognition. */
    memset(stack_registers, 0xeeeeeeee, sizeof(stack_registers_t));
    stack_registers->r8 = (register_t)(0x88888888);
    stack_registers->r5 = (register_t)(0x55555555);

    stack_registers->pc = (register_t)(exec_from)| 1;
    stack_registers->r0 = 0x00000000;
    stack_registers->r1 = 0x11111111;
    stack_registers->r2 = 0x22222222;
    stack_registers->r3 = 0x33333333;

    stack_registers->r12 = 0x12121212;
    stack_registers->lr = 0xdeadbeef;

    /* TODO: figure out a (more) correct value for PSR for userprograms. */
    stack_registers->psr = 0x61000000;

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
int
main(void)
{
    /*
     * Initialize the zone allocator.
     */
    zinit();

    /*
     * Create a "dummy" one-time-use PCB to jumpstart scheduling.
     */
    kzone_pcb = zalloc(KZONE_PCB);
    assert(kzone_pcb != NULL);
    pcb_active = kzone_pcb;

    /* TODO: possibly make a "phony" kernel PCB for as the active PCB to
     * bootstrap scheduler.
     */

    /*
     * The heap will begin aligned with the first MPU-protectable address after
     * the kernel private state section ends. The heaps begins as a singleton
     * free element.
     */
    heap_start = &__bss_end__ + ((MPU_REGION_GRANULARITY) -
        (uint32_t)&__bss_end__ % (MPU_REGION_GRANULARITY));
    heap_free_list = (heap_region_t *)heap_start;
    heap_free_list->next = NULL;
    heap_free_list->prev = NULL;
    heap_free_list->size = (SRAM_START + SRAM_SIZE) - (uint32_t)heap_start - sizeof(heap_region_t);

    /* Create resources for two pre-loaded programs, already present in flash
     * at addresses 0x10020000 and 0x10010000 respectively. Each has a different
     * entrypoint, discovered by reading the symbol tables in their .elf files.
     * Each program is given 60KB of space (more than enough).
     */
    create_system_resources((void *)0x10020000, (void *)0x20020000, (void *)0x20020298, (60 * 1024));
    create_system_resources((void *)0x10010000, (void *)0x20010000, (void *)0x20010298, (60 * 1024));

    /* Unify our stack pointers */
    asm("mrs r0, msp");
    asm("msr psp, r0");
    asm("isb");


    /* Register the schedule_handler as the timer interrupt handler. A voluntary
     * "yield" call could be implemented by using the `svc` instruction and
     * registering the schedule_handler as the SVCALL exception handler:
     * 
     *      exception_set_exclusive_handler(SVCALL_EXCEPTION, schedule_handler);
     */
    exception_set_exclusive_handler(SYSTICK_EXCEPTION, schedule_handler);

    /* Set SYST_RVR timer reset value */
    systick_hw->rvr = 0xffff;  /* TODO: configure this value */

    /* Configure and enable SysTick counter */
    systick_hw->csr = 0x7;

    /* Timer interrupt will deschedule this process and never reschedule it. */
    while (1) {}
}

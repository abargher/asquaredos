#define KERNEL

#include <string.h>
#include <stdio.h>
#include "boot.h"
#include "utils/list.h"
#include "utils/panic.h"
#include "palloc.h"
#include "zalloc.h"
#include "context_switch.h"
#include "pico/stdlib.h"
#include "hardware/exception.h"
#include "scheduler.h"
#include "hardware/structs/systick.h"

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
extern char __bss_end__;

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

    printf("line %d\n", __LINE__);
    pcb_t *pcb = (pcb_t *)zalloc(KZONE_PCB);
    assert(pcb != NULL);
    printf("line %d\n", __LINE__);

    /*
     * The PCB starts with no allocated memory.
     */
    pcb->allocated = NULL;

    /*
     * Allocate a stack.
     */
    printf("line %d\n", __LINE__);
    void *stack = palloc(STACK_SIZE, pcb, PALLOC_FLAGS_ANYWHERE, NULL);
    printf("stack: %p - %p\n", stack, stack+STACK_SIZE);
    assert(stack != NULL);
    pcb->saved_sp = (uint32_t)stack + STACK_SIZE;   /* Start at TOP of the stack. */

    /*
     * Allocate space for the program's execution state ("binary") in memory.
     */
    // if (load_to == (void *)0x20020000) {
        void *out = palloc(load_size, pcb, PALLOC_FLAGS_FIXED, load_to);
        printf("bin: %p - %p\n", out, out+load_size);
        assert(out == load_to);
        memcpy(load_to, load_from, load_size);
    // }

    /*
     * Align stack pointer correctly pop our initial saved registers.
     */
    pcb->saved_sp -= sizeof(stack_registers_t);

    /*
     * Resume execution at the beginning of the binary.
     */

    stack_registers_t *stack_registers = (stack_registers_t *)pcb->saved_sp;
    memset(stack_registers, 0xeeeeeeee, sizeof(stack_registers_t));
    // stack_registers->lr = (register_t)load_to | 1;
    stack_registers->exc_return = (register_t)0xfffffffd;
    stack_registers->r8 = (register_t)(0x88888888);
    stack_registers->r5 = (register_t)(0x55555555);

    stack_registers->pc = (register_t)load_to | 1;
    /* TODO: What is going on here? 
        When exception return path executes, value of PC is off by 2 registers
        worth (8 bytes). Removing these two registers works, and all regs have
        correct values. Are there two extra uint32_ts pushed to the stack?

        See here:
        https://developer.arm.com/documentation/dui0497/a/the-cortex-m0-processor/exception-model/exception-entry-and-return
    */
    // stack_registers->r0 = 0x00000000;
    // stack_registers->r1 = 0x11111111;
    stack_registers->r2 = 0x22222222;
    stack_registers->r3 = 0x33333333;

    stack_registers->r12 = 0x12121212;
    stack_registers->lr = 0xdeadbeef;


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
    stdio_init_all();
    sleep_ms(5000);
    printf("\n\n\n\n==================================\n");
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

    /* TODO: make a "phony" kernel PCB for as the active PCB to bootstrap scheduler. */

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
    // heap_free_list->size = (SRAM_SIZE) - (uint32_t)(&__bss_end__ - (SRAM_START));
    heap_free_list->size = (SRAM_START + SRAM_SIZE) - (uint32_t)heap_start - sizeof(heap_region_t);
    printf("initial free list block: [%p, %p, %p]\n", heap_free_list, heap_free_list->data, heap_free_list->data + heap_free_list->size);

    /* Get programs to run, and their sizes..? */


    printf("pcb active: %p\n", pcb_active);
    create_system_resources((void *)0x10020000, (void *)0x20020000, (60 * 1024));

    printf("bin_start: %lx\n", *(long *)(0x20020000));

    printf("pcb active: %p\n", pcb_active);
    create_system_resources((void *)0x10010000, (void *)0x20010000, (60 * 1024));

    printf("bin_start: %lx\n", *(long *)(0x20020000));

    printf("pcb active: %p\n", pcb_active);
    printf("ready queue: %p\n", ready_queue);

    /* Unify our stack pointers */
    asm("mrs r0, msp");
    asm("msr psp, r0");

    // exception_set_exclusive_handler(SVCALL_EXCEPTION, schedule_handler);
    exception_set_exclusive_handler(SYSTICK_EXCEPTION, schedule_handler);

    /* Set SYST_RVR timer reset value */
    systick_hw->csr = 0x7;
    systick_hw->rvr = 0xffff;

    printf("systick_hw->csr: 0x%x\n", systick_hw->csr);
    printf("systick_hw->rvr: 0x%x\n", systick_hw->rvr);

    // hw_set_bits((void *)0xE000E010, *(uint32_t *)(0xE000E010) | 0b11);

    while (1) {}
    asm("svc #0");

    // p/x *(stack_registers_t * )($r2)
    //
    // schedule_handler();
}

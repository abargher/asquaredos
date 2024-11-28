#define KERNEL

#include <string.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/exception.h"
#include "hardware/structs/systick.h"
#include "hardware/structs/mpu.h"

#include "boot.h"
#include "utils/list.h"
#include "palloc.h"
#include "zalloc.h"
#include "context_switch.h"
#include "scheduler.h"
#include "vm.h"
#include "third-party/m0FaultDispatch.h"


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
     * Allocate and initialize a page table.
     */
    pcb->page_table = zalloc(KZONE_PTE_GROUP_TABLE);
    assert(pcb->page_table != NULL);
    for (int i = 0; i < GROUP_SIZE; i++) {
        pcb->page_table->groups[i] = PTE_GROUP_INVALID;
    }

    /*
     * Load the program's binary into memory. We'll evict any overlapped pages,
     * copy in the binary, and then allocate and configure the corresponding
     * PTEs.
     */
    for (unsigned int i = 0; i <= load_size / PAGE_SIZE; i++) {
        vm_map_generic_flash_page(
            pcb->page_table,
            load_from + i * PAGE_SIZE,
            load_to + i * PAGE_SIZE);
    }

    /*
     * Stack begins at the top of memory.
     */
    pcb->saved_sp = (register_t)VM_END;

    /*
     * Align stack pointer correctly to pop our initial saved registers.
     */
    pcb->saved_sp -= sizeof(stack_registers_t);

    /* XXX TODO XXX
     *
     * We need to write to a location in the program's address space in order to
     * initialize their stack. This means setting up PTEs that map to some
     * place that has the initial values for the stack.
     *
     * XXX TODO XXX
     * 
     * Also a note: what happens when we return from the schedule handler
     *              exception, and the hardware tries to pop registers from
     *              the user's stack? Unless we change something, the stack
     *              won't be paged in yet, so a hardfault is going to occur,
     *              (probably..? unless the pop occurs in handler mode?) which
     *              will result in the data being read in and us attempting to
     *              re-execute the saved PC-2, but this may not be well defined,
     *              and it may not be so trivial to recover from a hardfault
     *              that occurs in the middle of a hardware register restore.
     * 
     *              A potential solution here is to always page in the stack of
     *              the to-be-scheduled program when a context switch occurs. Or
     *              at least all of the stack that's above the stack pointer.
     *              --> This is probably what we should do.
     */

    /*
     * Configure the initial register values.
     */
    stack_registers_t *stack_registers = (stack_registers_t *)pcb->saved_sp;
    memset(stack_registers, 0, sizeof(stack_registers_t));
    stack_registers->pc = (register_t)(exec_from)| 1;
    stack_registers->psr = 0x61000000; /* TODO: figure out a (more) correct value for PSR for userprograms. */

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
    stdio_init_all();
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

    /*
     * Initialize the VM system.
     */
    vm_init();

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
    // create_system_resources((void *)0x10020000, (void *)0x20020000, (void *)0x20020298, (60 * 1024));
    create_system_resources((void *)0x10010000, (void *)0x20010000, (void *)0x20010298, (60 * 1024));

    /* Unify our stack pointers */
    asm("mrs r0, msp");
    asm("msr psp, r0");
    asm("isb");

    exception_set_exclusive_handler(HARDFAULT_EXCEPTION, HardFault_Handler);

    /* Register the schedule_handler as the timer interrupt handler. A voluntary
     * "yield" call could be implemented by using the `svc` instruction and
     * registering the schedule_handler as the SVCALL exception handler:
     * 
     *      exception_set_exclusive_handler(SVCALL_EXCEPTION, schedule_handler);
     */
    // exception_set_exclusive_handler(SYSTICK_EXCEPTION, schedule_handler);
    exception_set_exclusive_handler(SVCALL_EXCEPTION, schedule_handler);

    /* Set SYST_RVR timer reset value */
    systick_hw->rvr = 0xffff;  /* TODO: configure this value */

    /* Configure and enable SysTick counter */
    // systick_hw->csr = 0x7;

    /* Timer interrupt will deschedule this process and never reschedule it. */
    asm("svc #0");
    while (1) {}
}

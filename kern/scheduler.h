/*
 * scheduler.h
 *
 */

#ifndef __SCHED_H__
#define __SCHED_H__
#include <stdint.h>
#include "vm.h"

/*
 * 32-bit register value.
 */
typedef uint32_t register_t;

/*
 * Represents a single region of memory that has been allocated to, or could
 * be allocated to a process, by the kernel.
 */
typedef struct heap_region {
    uint32_t size;   /* Size of region in bytes. */

    /*
     * Multiplexed between global free list when not allocated, and the kernel's
     * private accounting of a process's allocated list when the region has been
     * allocated.
     */
    struct heap_region     *next;
    struct heap_region     *prev;

    uint8_t data[];     /* Beginning of region's user data. */
} heap_region_t;

/*
 * The format in which registers are pushed to the stack during a context
 * switch, allowing easy access to these values when creating a process.
 * 
 * The saved stack pointer will be located in the process's PCB.
 */
__attribute__((packed))
struct stack_registers {
    register_t r8;
    register_t r9;
    register_t r10;
    register_t r11;
    
    register_t r4;
    register_t r5;
    register_t r6;
    register_t r7;

    /* Hardware loaded registers */
    register_t r0;
    register_t r1;
    register_t r2;
    register_t r3;
    register_t r12;
    register_t lr;
    register_t pc;
    register_t psr;
};
typedef struct stack_registers stack_registers_t;

/*
 * Process control block containing the data and references required to manage
 * a running process.
 */
typedef struct process_control_block {
    register_t          saved_sp;       /* Saved stack pointer to recover other registers. */
    heap_region_t      *allocated;      /* List of allocated heap regions. */
    pte_group_table_t   page_table;     /* Base of the process' page table. */

    /*
     * Queue management fields.
     */
    struct process_control_block *next;
    struct process_control_block *prev;
} pcb_t;

#endif /* __SCHED_H__ */

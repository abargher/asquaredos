/*
 * scheduler.h
 *
 */

#ifndef __SCHED_H__
#define __SCHED_H__

#include "palloc.h"

/*
 * 32-bit register value.
 */
typedef uint32_t register_t;

/*
 * The format in which registers are pushed to the stack during a context
 * switch, allowing easy access to these values when creating a process.
 * 
 * The saved stack pointer will be located in the process's PCB.
 */
__attribute__((packed))
typedef struct {
    /*
     * Saved by initial "ldmia" instruction.
     */
    register_t lr;  /* Link register. Will be loaded into PC. */
    register_t r3;
    register_t r2;
    register_t r1;
    register_t r0;
    
    /*
     * Saved by the first "push" instruction.
     */
    register_t r7;
    register_t r6;
    register_t r5;
    register_t r4;

    /*
     * Saved by the second "push" instruction.
     */
    register_t r11;
    register_t r10;
    register_t r9;
    register_t r8;
} stack_registers_t;

/*
 * Process control block containing the data and references required to manage
 * a running process.
 */
typedef struct process_control_block {
    register_t      saved_sp;       /* Saved stack pointer to recover other registers. */
    heap_region_t  *allocated;      /* List of allocated heap regions. */

    /*
     * Queue management fields.
     */
    struct process_control_block *next;
    struct process_control_block *prev;
} pcb_t;

#endif /* __SCHED_H__ */

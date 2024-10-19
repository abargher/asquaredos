#ifndef __BOOT_H__
#define __BOOT_H__

#include "../user/allocator.h"

typedef uint32_t register_t;    /* 32-bit register. */

typedef struct process_control_block {
    /*
     * Context switch state.
     */
    struct {
        /*
         * General-purpose registers.
         */
        register_t r0;
        register_t r1;
        register_t r2;
        register_t r3;
        register_t r4;
        register_t r5;
        register_t r6;
        register_t r7;
        register_t r8;
        register_t r9;
        register_t r10;
        register_t r11;
        register_t r12;

        /*
         * Special-purpose registers.
         */
        register_t sp;          /* Register 13. Stack pointer. */
        register_t lr;          /* Register 14. Link register. */
        register_t pc;          /* Register 15. Program counter. */
        register_t psr;         /* Register 16. Program status register. */
        register_t primask;     /* Register 17. Priority mask. */
        register_t control;     /* Register 18. Privilege mode. */
    } registers;

    heap_region_t                  *allocated;  /* List of allocated heap regions. */
    struct process_control_block   *sched_next; /* Round-robin next process. */
} pcb_t;

#endif /* __BOOT_H__ */

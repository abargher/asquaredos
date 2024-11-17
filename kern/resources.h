/*
 * resources.h:
 *
 * This file defines and provides convenient access to the fixed-size set of
 * kernel-private resources allocated at boot-time.
 */

#ifndef __RESOURCES_H__
#define __RESOURCES_H__

#include "palloc.h"
#include "scheduler.h"

#include <stdint.h>

#define KERNEL_STATE_SECTION "kernel_private_state"

/*
 * Reservation of global kernel variables.
 */
extern pcb_t           *pcb_active         ;//__attribute__((section(KERNEL_STATE_SECTION)));     /* PCB of the current process. */
extern pcb_t           *ready_queue        ;//__attribute__((section(KERNEL_STATE_SECTION)));     /* Scheduler's ready queue. */
extern heap_region_t   *heap_free_list     ;//__attribute__((section(KERNEL_STATE_SECTION)));     /* Free regions in the heap. */
extern void            *heap_start         ;//__attribute__((section(KERNEL_STATE_SECTION)));     /* Starting address of the heap. */

/*
 * Reservation of all memory (zones) belonging to the zone allocator.
 */
extern pcb_t           *kzone_pcb   ;//__attribute__((section(KERNEL_STATE_SECTION)));

#endif /* __RESOURCES_H__ */

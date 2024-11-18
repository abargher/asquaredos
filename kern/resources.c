#include "resources.h"

/*
 * Reservation of global kernel variables.
 */
pcb_t           *pcb_active         ;//__attribute__((section(KERNEL_STATE_SECTION)));     /* PCB of the current process. */
pcb_t           *ready_queue        ;//__attribute__((section(KERNEL_STATE_SECTION)));     /* Scheduler's ready queue. */
heap_region_t   *heap_free_list     ;//__attribute__((section(KERNEL_STATE_SECTION)));     /* Free regions in the heap. */
void            *heap_start         ;//__attribute__((section(KERNEL_STATE_SECTION)));     /* Starting address of the heap. */

/*
 * Reservation of all memory (zones) belonging to the zone allocator.
 */
pcb_t           *kzone_pcb;//__attribute__((section(KERNEL_STATE_SECTION)));

/*
This value returns to thread mode. See below for more:
https://developer.arm.com/documentation/dui0497/a/the-cortex-m0-processor/exception-model/exception-entry-and-return
*/
void *exc_return = (void *)0xFFFFFFFD;
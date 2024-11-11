/*
 * palloc.h:
 *
 */

#ifndef __PALLOC_H__
#define __PALLOC_H__

#include "resources.h"
#include "scheduler.h"

/*
 * Allocate memory for a userspace process.
 */
void *
palloc(uint32_t size, pcb_t *owner);

/*
 * Reclaim memory that was allocated to a userspace process. Pointer should be
 * to the data element of heap_region_t.
 */
void
pfree(void *ptr);

#endif /* __PALLOC_H__ */

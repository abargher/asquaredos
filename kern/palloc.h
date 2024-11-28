/*
 * palloc.h:
 *
 */

#ifndef __PALLOC_H__
#define __PALLOC_H__

#include "resources.h"
#include "scheduler.h"

#define PALLOC_FLAGS_NONE 0
#define PALLOC_FLAGS_ANYWHERE 0
#define PALLOC_FLAGS_FIXED 1

/*
 * Allocate memory for a userspace process.
 *
 * Acceptable flags:
 *   PALLOC_ANYWHERE    allow any valid address to be returned
 *   PALLOC_FIXED       allow only an address starting at hint to be returned
 */
void *
palloc(uint32_t size, pcb_t *owner, int flags, void *hint);

/*
 * Reclaim memory that was allocated to a userspace process. Pointer should be
 * to the data element of heap_region_t.
 */
void
pfree(void *ptr, pcb_t *owner);

#endif /* __PALLOC_H__ */

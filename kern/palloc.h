/*
 * palloc.h:
 *
 */

#ifndef __PALLOC_H__
#define __PALLOC_H__

#include "resources.h"

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

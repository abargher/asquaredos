#ifndef __ALLOCATOR_H__
#define __ALLOCATOR_H__

#include "../shared/shared.h"

typedef struct heap_region {
    uint32_t                size;
    struct heap_region     *next;
    struct heap_region     *prev;
    uint8_t                 data[];
} heap_region_t;

#ifdef KERNEL
/*
 * Kernel allocate. Allows caller to specify owner of returned heap region.
 * Symbol only exported in kernel context.
 */
inline
void *
kalloc(uint32_t size, pcb_t *owner);
#endif

/*
 * Userspace allocate. Owner is determined by the active process control block.
 */
void *
malloc(uint32_t size);

/*
 * Free a heap region. Pointer should be to DATA element of heap_region_t.
 */
void
free(void *ptr);

#endif /* __ALLOCATOR_H__ */

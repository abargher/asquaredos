#include "allocator.h"

/*
 * Kernel allocate. Allows caller to specify owner of returned heap region.
 * Symbol only exported in kernel context.
 */
#ifdef KERNEL
inline
#else
static
#endif
void *
kalloc(uint32_t size, pcb_t *owner)
{
    /* Traverse free list until sufficiently sized block is found. Break block
       to proper size, delink allocated block, link trimmed end of that block,
       and return allocated.data to user. Return NULL if free list empty/no fit. */
}

/*
 * Userspace allocate. Owner is determined by the active process control block.
 */
void *
malloc(uint32_t size)
{
    return kalloc(size, pcb_active);
}

/*
 * Free a heap region. Pointer should be to DATA element of heap_region_t.
 */
void
free(void *ptr)
{
    /* Delink from allocated list, insert into free list. */
}

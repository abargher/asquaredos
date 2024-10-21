#include "palloc.h"

void *
palloc(uint32_t size, pcb_t *owner)
{
    /* Traverse free list until sufficiently sized block is found. Break block
       to proper size, delink allocated block, link trimmed end of that block,
       and return allocated.data to user. Return NULL if free list empty/no fit. */
}

void
pfree(void *ptr)
{
    /* Delink from allocated list, insert into free list. */
}

#include "palloc.h"
#include <stdint.h>

#include "utils/list.h"

void *
palloc(uint32_t size, pcb_t *owner)
{
    heap_region_t *out = NULL;

    heap_region_t *cur;
    for (cur = heap_free_list; cur; cur = cur->next) {
        if (out->size >= size) {
            DLL_REMOVE(heap_free_list, out, next, prev);
            break;
        }
    }
    if (out == NULL) {
        return NULL;
    }

    /*
     * Assuming we found a block, trim the end and put it back into the free
     * list if there's sufficient leftover space.
     */
    if (out && out->size >= size + sizeof(heap_region_t)) {
        heap_region_t *leftover = (heap_region_t *)((void *)out + size);
        leftover->size = out->size - size;
        DLL_INSERT(heap_free_list, out->prev, out + size, next, prev);
    }

    return out->data;
}

void
pfree(void *ptr, pcb_t *owner)
{
    /*
     * Delink from the allocated list. Note that metadata is stored as a prefix
     * to the heap region.
     */
    heap_region_t *region = (heap_region_t *)(ptr - sizeof(heap_region_t));
    DLL_REMOVE(owner->allocated, region, next, prev);

    /*
     * Insert into the free list in order and coalesce adjacent blocks.
     */
    heap_region_t *cur;
    for (cur = heap_free_list; cur; cur = cur->next) {
        /*
         * Keep walking until we find a region after the one we want to insert,
         * or until we've hit the last region in the list.
         */
        if (cur > region) {
            cur = cur->prev;
            break;
        }
        if (cur->next == NULL) {
            break;
        }
    }

    /*
     * Now we have the we want to insert after. Before we insert, check if we
     * are adjacent to, and can coalesce with, that element.
     */
    if (cur + cur->size == region) {
        cur->size += region->size;
        region = cur;
    } else {
        DLL_INSERT(heap_free_list, cur, region, next, prev);
    }

    /*
     * Now check if we can coalesce with the following element.
     */
    if (cur->next && cur + cur->size == cur->next) {
        cur->size += cur->next->size;
        DLL_REMOVE(heap_free_list, cur->next, next, prev);
    }
}

#include "palloc.h"
#include <stdint.h>
#include <stdio.h>

#include "utils/list.h"
#include "utils/panic.h"
#include "resources.h"
#include <stddef.h>

/*
 * Find and delink any block containing size bytes at any address.
 */
heap_region_t *
palloc_find_anywhere(uint32_t size)
{
    heap_region_t *out = NULL;

    for (out = heap_free_list; out; out = out->next) {
        if (out->size >= size) {
            DLL_REMOVE(heap_free_list, out, next, prev);
            break;
        }
    }
    
    return out;
}

/*
 * Find and delink the block containing size bytes at the specified address.
 * Fails and returns NULL if 
 */
heap_region_t *
palloc_find_fixed(
    uint32_t    size,
    void       *address)
{
    heap_region_t *out = NULL;
    for (out = heap_free_list; out; out = out->next) {
        /*
         * Check that the current block contains the entire requested region.
         */
        if (out->data <= (uint8_t*)address &&
            out->data + out->size >= (uint8_t*)address + size) {
            DLL_REMOVE(heap_free_list, out, next, prev);
            break;
        }
    }
    if (out == NULL) {
        return NULL;
    }

    /*
     * Trim the beginning and re-link it to simplify things for the caller and
     * have both the _fixed and _anywhere variants do the same thing. 
     */
    if (out->data != address) {
        /*
         * Trims OUT into START and OUT', as depicted below, and re-links START,
         * and updates the backpointer in OUT'.
         *            [<-HINT->]
         * [<---------OUT--------->]
         * [<-START->][<---OUT'--->]
         */
        heap_region_t *start = out;
        out = ((heap_region_t *)address) - 1;
        assert(out > start);        /* TODO handle this gracefully later by returning an error. */

        out->size = start->size;

        /*
         * Free the trimmed head.
         */

        start->size = (uint32_t)(address - (void *)&start->data); /* <-- TODO fix me. */
        start->prev = NULL;

        DLL_INSERT(heap_free_list, start->prev, start, next, prev);

        out->size -= start->size;
        out->prev = start;
    }

    return out;
}

void *
palloc(
    uint32_t    size,
    pcb_t      *owner,
    int         flags,
    void       *hint)
{
    /*
     * Find the block we're going to give memory from.
     */
    heap_region_t *out = flags & PALLOC_FLAGS_FIXED ?
                         palloc_find_fixed(size, hint) :
                         palloc_find_anywhere(size);
    if (out == NULL) {
        return NULL;
    }

    /*
     * Assuming we found a block, trim the end and put it back into the free
     * list if there's sufficient leftover space.
     */
    if (out && out->size >= size + sizeof(heap_region_t)) {
        heap_region_t *leftover = (heap_region_t *)((void *)out + size); /* <-- TODO fix me. */
        leftover->size = out->size - size;
        DLL_INSERT(heap_free_list, out->prev, (heap_region_t *)((void *)out->data + size), next, prev);
    }

    return out->data;
}

void
pfree(
    void   *ptr,
    pcb_t  *owner)
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

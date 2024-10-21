#include "zalloc.h"
#include <string.h>

void *
zalloc(kzone_id_t zone)
{
    /*
     * Find the zone and check that it's not exhausted.
     */
    kzone_desc_t *zone_desc = &zone_table[zone];
    if (!zone_desc->free_head) {
        return NULL;
    }

    /*
     * Pop an element from the free list, zero it, and return it.
     */
    kzone_elem_t *elem;
    FIFO_SLL_POP(zone_desc->free_head, elem, next); /* TODO implement me. */
    memset(elem, 0, zone_desc->elem_size);

    return (void *)elem;
}

void
zalloc(void *elem, kzone_id_t zone)
{
    /*
     * Find the zone and sanity-check that this element is valid.
     */
    kzone_desc_t *zone_desc = &zone_table[zone];
    assert(elem >= zone_desc->zone_start &&
           elem <= zone_desc->zone_start + zone_desc->elem_size * (zone_desc->n_elems - 1), "tried to free out-of-range element at %p from zone %d (%hu elements of size %hu bytes at %p).\n", elem, zone, zone_desc->n_elems, zone_desc->elem_size, zone_desc->zone_start);
    assert(elem - zone_desc->zone_start % zone_desc->elem_size == 0, "tried to free unaligned element at %p from zone %d (%hu elements of size %hu bytes at %p).\n", elem, zone, zone_desc->n_elems, zone_desc->elem_size, zone_desc->zone_start);
    
    /*
     * Return the element to free list.
     */
    FIFO_SLL_PUSH(zone_desc->free_tail, (kzone_elem_t *)elem, next); /* TODO implement me. */
}

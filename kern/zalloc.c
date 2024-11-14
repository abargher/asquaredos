#include "zalloc.h"
#include "scheduler.h"
#include "utils/list.h"
#include "utils/panic.h"
#include <string.h>

/*
 * State tracking for all allocator zones.
 */
kzone_desc_t zone_table[N_KZONES]; //__attribute__((section("kernel_private_state")));

/*
 * Create each zone.
 */
#define PCB_ZONE_ELEMS 32
pcb_t zone_pcbs[PCB_ZONE_ELEMS];


/*
 * Takes an otherwise initialized kzone_desc_t and sets up the free list.
 */
static void
initialize_zone_free_list(kzone_desc_t *desc)
{
    for (int i = 0; i < desc->n_elems; i++) {
        kzone_elem_t *elem = (kzone_elem_t *)(desc->zone_start + i * desc->elem_size);
        SLL_PUSH(desc->free_head, desc->free_tail, elem, next, prev);
    }
}

/*
 * Each zone needs an initialization call here. Adding any new zones
 * necesitates this registration so that the zone can be properly accessed.
 */
void
zinit(void)
{
    kzone_desc_t *desc;

    /*
     * Register the PCB zone.
     */
    desc = &zone_table[KZONE_PCB];
    desc->n_elems = PCB_ZONE_ELEMS;
    desc->elem_size = sizeof(pcb_t);
    desc->zone_start = (kzone_elem_t *)zone_pcbs;
    initialize_zone_free_list(desc);

    /*
     * Register the next zone here.
     */
}

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
    SLL_POP(zone_desc->free_head, zone_desc->free_tail, elem, next, prev);
    memset(elem, 0, zone_desc->elem_size);

    return (void *)elem;
}

void
zfree(
    void *elem,
    kzone_id_t zone)
{
    /*
     * Find the zone and sanity-check that this element is valid.
     */
    kzone_desc_t *zone_desc = &zone_table[zone];
    // assert(elem >= zone_desc->zone_start &&
    //        elem <= zone_desc->zone_start + zone_desc->elem_size * (zone_desc->n_elems - 1), "tried to free out-of-range element at %p from zone %d (%hu elements of size %hu bytes at %p).\n", elem, zone, zone_desc->n_elems, zone_desc->elem_size, zone_desc->zone_start);
    // assert((uint32_t)elem - (uint32_t)zone_desc->zone_start % zone_desc->elem_size == 0, "tried to free unaligned element at %p from zone %d (%hu elements of size %hu bytes at %p).\n", elem, zone, zone_desc->n_elems, zone_desc->elem_size, zone_desc->zone_start);

    /*
     * Return the element to free list.
     */
    SLL_PUSH(zone_desc->free_head, zone_desc->free_tail, (kzone_elem_t *)elem, next, prev);
}

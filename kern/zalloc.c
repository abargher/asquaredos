#include "zalloc.h"
#include "scheduler.h"
#include "utils/list.h"
#include <string.h>

/*
 * State tracking for all allocator zones.
 */
kzone_desc_t zone_table[N_KZONES];

/*
 * Define the size of each zone.
 */
#define PCB_ZONE_ELEMS (MAX_PROCESSES)
#define PTE_GROUP_ZONE_ELEMS (255)  /* Absolutely must not exceed 255. The index of the 256th element is reserved for PTE_GROUP_INVALID. */
#define PTE_GROUP_TABLE_ZONE_ELEMS (MAX_PROCESSES)

/*
 * Create each zone.
 */
pcb_t               zone_pcbs[PCB_ZONE_ELEMS];
pte_group_t         zone_pte_groups[PTE_GROUP_ZONE_ELEMS];
pte_group_table_t   zone_pte_group_tables[PTE_GROUP_TABLE_ZONE_ELEMS];

int x = sizeof(zone_pte_groups);
int y = sizeof(zone_pte_group_tables);
int z = sizeof(zone_pcbs);


/*
 * Export the base of the pte_group_t and pte_group_table_t zones. They are
 * required for looking up a PTE from an address.
 */
pte_group_t        *pte_groups_base         = zone_pte_groups - 1;
pte_group_table_t  *pte_group_tables_base   = zone_pte_group_tables;


/*
 * Initializes an individual zone.
 */
static void
initialize_zone(
    kzone_id_t  id,
    uint16_t    n_elems,
    uint16_t    elem_size,
    void       *start)
{
    kzone_desc_t *desc = &zone_table[id];
    desc->n_elems = n_elems;
    desc->elem_size = elem_size;
    desc->zone_start = (kzone_elem_t *)start;
    for (int i = 0; i < desc->n_elems; i++) {
        kzone_elem_t *elem = (kzone_elem_t *)(desc->zone_start + i * desc->elem_size);
        SLL_PUSH(desc->free_head, desc->free_tail, elem, next);
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

    initialize_zone(KZONE_PCB, PCB_ZONE_ELEMS, sizeof(pcb_t), zone_pcbs);
    initialize_zone(KZONE_PTE_GROUP, PTE_GROUP_ZONE_ELEMS, sizeof(pte_group_t), zone_pte_groups);
    initialize_zone(KZONE_PTE_GROUP_TABLE, PTE_GROUP_TABLE_ZONE_ELEMS, sizeof(pte_group_table_t), zone_pte_group_tables);
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
    SLL_POP(zone_desc->free_head, zone_desc->free_tail, elem, next);
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
    SLL_PUSH(zone_desc->free_head, zone_desc->free_tail, (kzone_elem_t *)elem, next);
}

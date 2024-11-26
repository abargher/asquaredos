/*
 * zalloc.h:
 *
 * This file defines the kernel zone allocator.
 */

#ifndef __ZALLOC_H__
#define __ZALLOC_H__

#include <stdint.h>

/*
 * Kernel zone allocator zone index.
 */
typedef enum {
   KZONE_PCB,       /* pcb_t */
   KZONE_PTE_GROUP,
   KZONE_PTE_GROUP_TABLE,
   N_KZONES
} kzone_id_t;

/*
 * Generic representation of a free element in a zone. When allocated, the
 * element construct is overwritten with the contents of a (presumably larger)
 * type.
 */
typedef struct kzone_element {
    struct kzone_element *next;     /* Next free element, if not allocated. */
} kzone_elem_t;

/*
 * Describes the contents and state of the zone for use by the allocator.
 */
typedef struct {
    uint16_t        n_elems;        /* Number of elements in this zone. */
    uint16_t        elem_size;      /* sizeof(type). */
    void           *zone_start;     /* First byte in the zone. */
    kzone_elem_t   *free_head;      /* First element in the free list (to pop). */
    kzone_elem_t   *free_tail;      /* Last element in the free list (to push). */
} kzone_desc_t;

/*
 * State tracking for all allocator zones.
 */
extern kzone_desc_t zone_table[N_KZONES];

/*
 * Initializes all zones for the zone allocator.
 */
void
zinit(void);

/*
 * Allocate a zeroed element from the specified zone. Returns NULL if zone is
 * exhausted.
 */
void *
zalloc(kzone_id_t zone);

/*
 * Returns the given element to the free list of the specified zone.
 */
void
zfree(void *elem, kzone_id_t zone);

#endif /* __ZALLOC_H__ */

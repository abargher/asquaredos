/*
 * vm.h:
 *
 * This file implements the internals of the virtual memory system.
 */

#ifndef __VM_H__
#define __VM_H__

/*
 * The first two bits of a PTE determine its type, which in turn determine how
 * the remainder of the PTE should be interpreted, according to where that PTE's
 * data resides (SRAM/write cache/flash).
 */
__attribute__((packed))
enum pte_type : unsigned char {     /* 8 bits. */
    PTE_INVALID,
    PTE_SRAM,
    PTE_CACHE,
    PTE_FLASH
};

/*
 * Page table entry for a 256B page stored in SRAM.
 *
 * An SRAM PTE will only be used for a page that has never been evicted, as
 * neither a cache/flash PTE may be promoted to SRAM PTE. Only demotion from
 * SRAM to cache/flash is allowable.
 */
__attribute__((packed))
struct sram_pte {                   /* 16 bits. */
    unsigned short _reserved:2;
    unsigned short _unused:14;
};

/*
 * Page table entry for a 256B page stored in the write cache.
 *
 * This page may ALSO be present in regular SRAM, without any additional
 * indication of such. If present in SRAM, the contents of the page in SRAM will
 * be compared to the contents pointed to by this PTE before overwriting.
 */
__attribute__((packed))
struct cache_pte {                  /* 16 bits. */
    unsigned short _reserved:2;
    unsigned short _unused:14;
};

/*
 * Page table entry for a 256B page stored in flash.
 *
 * This page may ALSO be present in regular SRAM, without any additional
 * indication of such. If present in SRAM, the contents of the page in SRAM will
 * be compared to the contents pointed to by this PTE. If the contents match,
 * the eviction will be silent. Otherwise, an entry in the write cache will be
 * procured, the contents of the page in SRAM will be written to it, and this
 * PTE will be promoted to PTE_CACHE.
 */
__attribute__((packed))
struct flash_pte {                  /* 16 bits. */
    unsigned short _reserved:2;
    unsigned short _unused:14;
};

__attribute__((packed))
typedef union {                     /* 16 bits. */
    enum    pte_type    type:2;
    struct  sram_pte    sram;
    struct  cache_pte   cache;
    struct  flash_pte   flash;
} pte_t;

#endif /* __VM_H__ */

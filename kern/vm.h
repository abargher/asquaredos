/*
 * vm.h:
 *
 * This file implements the internals of the virtual memory system.
 */

#ifndef __VM_H__
#define __VM_H__

#include <hardware/flash.h>

#define KB (1024)

/*
 * There is a 256KB (2^18 bytes) addressable range of SRAM. Addresses can thus
 * be treated as 18 bit after subtracting the base address of SRAM, and will be
 * interpreted as follows:
 * 
 *      00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17
 *      [    group:6    ] [ index:4 ] [    page offset:8    ]
 */

#define GROUP_BITS  (6)
#define INDEX_BITS  (4)
#define OFFSET_BITS (8)

#define PAGE_NUMBER_MASK    (0x3FF)
#define GROUP_MASK          (0x3F)
#define INDEX_MASK          (0x0F)
#define OFFSET_MASK         (0xFF)

#define PAGE_SIZE (1 << OFFSET_BITS)

/*
 * Write cache size is determined by number of pages it contains.
 */
#define WRITE_CACHE_BITS (16)
#define WRITE_CACHE_INDEX_BITS (WRITE_CACHE_BITS - OFFSET_BITS)
#define WRITE_CACHE_SIZE (1 << WRITE_CACHE_BITS)
#define WRITE_CACHE_NUM_ENTRIES (WRITE_CACHE_SIZE / PAGE_SIZE)

/*
 * Entire flash layout. We will allow special PTEs to be created that map from
 * a non-swap region of flash into SRAM, which means we need additional bit(s)
 * for the flash PTE index.
 */
#define FLASH_BASE (XIP_BASE)
#define FLASH_BITS (13)

/*
 * Flash swap region layout.
 */
#define FLASH_SWAP_BASE (XIP_BASE + (1 << 20))
#define FLASH_SWAP_BITS (12)
#define FLASH_SWAP_SIZE (1 << FLASH_SWAP_BITS + OFFSET_BITS)
#define FLASH_SWAP_NUM_PAGES (FLASH_SWAP_SIZE / PAGE_SIZE)
#define FLASH_SWAP_NUM_SECTORS (FLASH_SWAP_SIZE / FLASH_SECTOR_SIZE)
#define FLASH_SWAP_BASE_FLASH_OFS (FLASH_SWAP_BASE - XIP_BASE)

/*
 * Memory will be laid out as follows:
 *
 * LOW                                                                     HIGH
 *   [Kernel State][Write Cache][Main Memory until 256K][Kernel Stack @ 260K]
 * 
 * 
 */

/*
 * User-accessible memory starts after kernel .bss section ends, and continues
 * until the end of last 32KB SRAM bank.
 */
extern char __bss_end__;
#define VM_START ((void *)(((uintptr_t)&__bss_end__ | 0x1000) & ~(0xFFF))) /* 4KB-aligned. */
#define VM_END ((void *)SRAM_STRIPED_END)

/*
 * Process identifier. Valid values are 0-14, inclusive. Used by the address to
 * process conversion map.
 */
#define MAX_PROCESSES   (15)
#define PID_INVALID     ((pid_t)15)  /* Used to indicate page has no owner. */
#define PID_MIN         ((pid_t)0)
#define PID_MAX         ((pid_t)(MAX_PROCESSES - 1))
typedef unsigned char pid_t;

/*
 * The first two bits of a PTE determine its type, which in turn determine how
 * the remainder of the PTE should be interpreted, according to where that PTE's
 * data resides (SRAM/write cache/flash).
 */
typedef unsigned char pte_type_t;
#define PTE_INVALID (0b00)
#define PTE_SRAM    (0b01)
#define PTE_CACHE   (0b10)
#define PTE_FLASH   (0b11)

/*
 * 10-bit index of a page in SRAM.
 */
typedef unsigned short page_number_t;

/*
 * Convert a page_number_t to the address of the page it represents in memory.
 */
#define SRAM_PAGE(page_number) (SRAM_BASE + 256 * (page_number))

/*
 * Page table entry for a 256B page stored in SRAM.
 *
 * An SRAM PTE will only be used for a page that has never been evicted, as
 * neither a cache/flash PTE may be promoted to SRAM PTE. Only demotion from
 * SRAM to cache/flash is allowable.
 */
struct sram_pte {                   /* 16 bits. */
    unsigned short _reserved    :2;
    unsigned short _unused      :4;
    page_number_t  page_number  :10;
} __attribute__((packed));

/*
 * Index of a page in the write cache.
 */
typedef unsigned char cache_index_t;

/*
 * Convert a cache_index_t to the address of the page it represents.
 */
#define CACHE_PAGE(cache_index) (((void *)write_cache) + (PAGE_SIZE * (cache_index)))

/*
 * Page table entry for a 256B page stored in the write cache.
 *
 * This page may ALSO be present in regular SRAM, without any additional
 * indication of such. If present in SRAM, the contents of the page in SRAM will
 * be compared to the contents pointed to by this PTE before overwriting.
 */
struct cache_pte {                  /* 16 bits. */
    unsigned char   _reserved   :2;
    unsigned char   _unused     :3;
    unsigned char   dirty       :3; /* Increment when page written or re-written
                                       and decremented by eviction clock. Used
                                       to approximate which pages are most
                                       frequently dirtied, so that we evict the
                                       pages which are least frequently dirtied. */
    cache_index_t   cache_index :WRITE_CACHE_INDEX_BITS;
} __attribute__((packed));

/*
 * FLASH_SWAP_BITS-bit index of a page in the flash swap region.
 */
typedef unsigned short flash_index_t;

/*
 * Convert a flash_index_t to the memory mapped address of the page in flash
 * that it represents.
 */
#define FLASH_PAGE(flash_index) (FLASH_BASE + 256 * (flash_index))
#define FLASH_INDEX(page) ((((uintptr_t)page) - FLASH_BASE) / PAGE_SIZE)
#define FLASH_INDEX_FROM_SWAP_INDEX(swap_index) ((swap_index) | (1 << FLASH_BITS))

/*
 * Convert a flash_index_t to the byte offset into flash of the page that it
 * represents.
 */
#define FLASH_OFFSET(flash_index) (256 * (flash_index))

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
struct flash_pte {                  /* 16 bits. */
    unsigned short _reserved    :2;
    unsigned short _unused      :1;
    unsigned short flash_index  :FLASH_BITS;
} __attribute__((packed));

typedef union {                     /* 16 bits. */
    pte_type_t          type:2;
    struct sram_pte     sram;
    struct cache_pte    cache;
    struct flash_pte    flash;
} __attribute__((packed)) pte_t;

/*
 * The GROUP, INDEX, and OFFSET macros all assume that the address has already
 * been verified as in-range as an SRAM address.
 */
#define PAGE_NUMBER(addr)   (((unsigned)(addr) >> OFFSET_BITS) & PAGE_NUMBER_MASK)
#define GROUP(addr)         (((unsigned)(addr) >> (OFFSET_BITS + INDEX_BITS) & GROUP_MASK))
#define INDEX(addr)         (((unsigned)(addr) >> OFFSET_BITS) & INDEX_MASK)
#define OFFSET(addr)        ((unsigned)(addr) & OFFSET_MASK)

#define GROUP_INDEX(addr) ((pte_group_index_t)((pte_group_t *)(addr) - (pte_group_t *)pte_groups_base))

/*
 * A single 256 byte page. Can be present in SRAM, write cache, or flash.
 */
typedef struct {
    unsigned char data[PAGE_SIZE];
} page_t;

/*
 * A group of zone-allocated PTEs. Indexed by the "index" bits of an address.
 * This is the second level of a process' page table.
 */
#define GROUP_SIZE (1 << INDEX_BITS)
typedef struct {
    pte_t ptes[GROUP_SIZE];
} pte_group_t;

/*
 * Each process is zone-allocated a single PTE group table, which contains the
 * locations of (up to) 64 PTE group structs. This is the first level of the
 * process' page table.
 * 
 * Note that the group index is only 8 bits. This is because the PTE group zone
 * must not contain more than 256 elements.
 */
#define N_GROUPS (1 << GROUP_BITS)
#define PTE_GROUP_INVALID (0xFF)    /* Reserved index; zone only contains 255 elements. */
typedef unsigned char pte_group_index_t;
typedef struct {
    pte_group_index_t groups[N_GROUPS];
} pte_group_table_t;

#define PID_FROM_GROUP_TABLE(group_table_ptr) ((pid_t)((pte_group_table_t *)(group_table_ptr) - (pte_group_table_t *)pte_group_tables_base))

/*
 * Exported functions.
 */

void vm_init(void);
void vm_map_generic_flash_page(pte_group_table_t *pt, page_t *src, page_t *dst);
void vm_page_fault(pte_group_table_t *pt, void *addr);  /* Kernel needs to be able to fault in a process' stack during creation and during a context switch. */

#endif /* __VM_H__ */

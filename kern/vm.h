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

#define GROUP_BITS  6
#define INDEX_BITS  4
#define OFFSET_BITS 8

#define PAGE_NUMBER_MASK    0x3FF
#define GROUP_MASK          0x3F
#define INDEX_MASK          0x0F
#define OFFSET_MASK         0xFF

#define PAGE_SIZE (1 << OFFSET_BITS)

/*
 * Write cache size is determined by number of pages it contains.
 */
#define WRITE_CACHE_BITS (16)
#define WRITE_CACHE_INDEX_BITS (WRITE_CACHE_BITS - OFFSET_BITS)
#define WRITE_CACHE_SIZE (1 << WRITE_CACHE_BITS)
#define WRITE_CACHE_NUM_ENTRIES (WRITE_CACHE_SIZE / PAGE_SIZE)

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
 * Process identifier. Valid values are 1-15, inclusive. Used by the address to
 * process conversion map.
 */
#define MAX_PROCESSES   16
#define PID_INVALID     ((pid_t)0)  /* Used to indicate page has no owner. */
#define PID_MIN         ((pid_t)1)
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
#define SRAM_PAGE(page_number) (SRAM_BASE + 256 * page_number)

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
#define CACHE_PAGE(cache_index) (write_cache + 256 * cache_index)

/*
 * Page table entry for a 256B page stored in the write cache.
 *
 * This page may ALSO be present in regular SRAM, without any additional
 * indication of such. If present in SRAM, the contents of the page in SRAM will
 * be compared to the contents pointed to by this PTE before overwriting.
 */
__attribute__((packed))
struct cache_pte {                  /* 16 bits. */
    unsigned char   _reserved   :2;
    unsigned char   _unused     :3;
    unsigned char   dirty       :3; /* Increment when page written or re-written
                                       and decremented by eviction clock. Used
                                       to approximate which pages are most
                                       frequently dirtied, so that we evict the
                                       pages which are least frequently dirtied. */
    cache_index_t   cache_index :WRITE_CACHE_INDEX_BITS;
};

/*
 * FLASH_SWAP_BITS-bit index of a page in the flash swap region.
 */
typedef unsigned short flash_index_t;

/*
 * Convert a flash_index_t to the memory mapped address of the page in flash
 * that it represents.
 */
#define FLASH_PAGE(flash_index) (FLASH_SWAP_BASE + 256 * flash_index)

/*
 * Convert a flash_index_t to the byte offset into flash of the page that it
 * represents.
 */
#define FLASH_OFFSET(flash_index) (FLASH_SWAP_BASE_FLASH_OFS + 256 * flash_index)

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
    unsigned short _unused      :2;
    unsigned short flash_index  :FLASH_SWAP_BITS;
} __attribute__((packed));

typedef union {                     /* 16 bits. */
    pte_type_t          type:2;
    struct sram_pte     sram;
    struct cache_pte    cache;
    struct flash_pte    flash;
} __attribute__((packed)) pte_t;

/*
 * MPU region details.
 */
#define MPU_REGION_BITS (15)
#define MPU_SUBREGION_BITS (MPU_REGION_BITS - 3)        /* 12 bits. */

#define MPU_REGION_SIZE (1 << MPU_REGION_BITS)          /* 32KB. */
#define MPU_SUBREGION_SIZE (1 << MPU_SUBREGION_BITS)    /* 4KB. */

#define MPU_REGION_BASE(addr) (typeof(addr))(((unsigned)addr >> MPU_REGION_BITS) << MPU_REGION_BITS)
#define MPU_SUBREGION_BASE(addr) (typeof(addr))(((unsigned)addr >> MPU_SUBREGION_BITS) << MPU_SUBREGION_BITS)

/*
 * The GROUP, INDEX, and OFFSET macros all assume that the address has already
 * been verified as in-range as an SRAM address.
 */
#define PAGE_NUMBER(addr)   (((unsigned)addr >> OFFSET_BITS) & PAGE_NUMBER_MASK)
#define GROUP(addr)         (((unsigned)addr >> (OFFSET_BITS + INDEX_BITS) & GROUP_MASK))
#define INDEX(addr)         (((unsigned)addr >> OFFSET_BITS) & INDEX_MASK)
#define OFFSET(addr)        ((unsigned)addr & OFFSET_MASK)

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
typedef unsigned char pte_group_index_t;
typedef struct {
    pte_group_index_t groups[N_GROUPS];
} pte_group_table_t;

#endif /* __VM_H__ */

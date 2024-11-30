/*
 * mpu.h:
 *
 * Abstracted interface to handle configuration of the MPU.
 */

#ifndef __MPU_H__
#define __MPU_H__

#include <stdbool.h>

/*
 * Allowed values for the AP field of MPU_RASR. Named as:
 *
 *  MPU_AP__<PRIV>_<UNPRIV>
 */
#define MPU_AP__NA_NA (0b000)   /* All accesses generate a permission fault. */
#define MPU_AP__RW_NA (0b001)   /* Access from privileged software only. */
#define MPU_AP__RW_RO (0b010)   /* Writes by unprivileged software generate a permission fault. */
#define MPU_AP__RW_RW (0b011)   /* Full access. */
#define MPU_AP__RO_NA (0b101)   /* Reads by privileged software only. */
#define MPU_AP__RO_RO (0b110)   /* Read only, by privileged or unprivileged software. */

/*
 * Layout of the MPU_RASR register.
 */
typedef struct {
    unsigned int enable             :1; /* Region enable bit. */
    unsigned int size               :5; /* Specified the size of the MPU region (2^SIZE+1). Minimum permitted value is 7. */
    unsigned int /* reserved. */    :2;
    unsigned int srd                :8; /* Subregion disable maps; 0 -> sr enabled; 1 -> sr disabled. */
    unsigned int bufferable         :1; /* Bufferable bit. */
    unsigned int cacheable          :1; /* Cacheable bit. */
    unsigned int shareable          :1; /* Shareable bit. */
    unsigned int /* reserved. */    :5;
    unsigned int ap                 :3; /* Access permission field. */
    unsigned int /* reserved. */    :1;
    unsigned int xn                 :1; /* Instruction access bit. */
    unsigned int /* reserved. */    :3;
} mpu_rasr_t;

/*
 * Layout of the MPU_RBAR register.
 */
typedef struct {
    unsigned int region             :4;     /* On writes, behavior depends on VALID field. On reads, returns current region number. */
    unsigned int valid              :1;     /* MPU region number valid bit; 0 = use MPU_RNR; 1 = set MPU_RNR to REGION field. */
    unsigned int /* reserved. */    :10;
    unsigned int addr               :17;    /* Region base address field. Bits [31:N], where N=log2(region size) = 15. */
} mpu_rbar_t;

/*
 * Layout of the MPU_RNR register.
 */
typedef struct {
    unsigned int region             :8;     /* Indicates the MPU region referenced by MPU_RBAR and MPU_RASR registers. Permitted values are 0-7. */
    unsigned int /* reserved. */    :24;
} mpu_rnr_t;

/*
 * Layout of the MPU_CTRL register.
 */
typedef struct {
    unsigned int enable             :1;     /* Enables the MPU. */
    unsigned int hfnmiena           :1;     /* Enables the operation of MPU during HardFault and NMI handlers. */
    unsigned int privdefena         :1;     /* Enables privileged software access to the default memory map. */
    unsigned int /* reserved. */    :29;
} mpu_ctrl_t;

#define MPU_NUM_REGIONS (8)

/*
 * MPU region details.
 */
#define MPU_REGION_BITS (15)
#define MPU_SUBREGION_BITS (MPU_REGION_BITS - 3)        /* 12 bits. */

#define MPU_REGION_SIZE (1 << MPU_REGION_BITS)          /* 32KB. */
#define MPU_SUBREGION_SIZE (1 << MPU_SUBREGION_BITS)    /* 4KB. */

#define MPU_REGION_BASE(addr) (typeof(addr))(((unsigned)(addr) >> MPU_REGION_BITS) << MPU_REGION_BITS)
#define MPU_SUBREGION_BASE(addr) (typeof(addr))(((unsigned)(addr) >> MPU_SUBREGION_BITS) << MPU_SUBREGION_BITS)

#define MPU_REGION(addr) (((unsigned int)addr >> MPU_REGION_BITS) & 0x7)
#define MPU_SUBREGION(addr) (((unsigned int)addr >> MPU_SUBREGION_BITS) & 0x7)

/*
 * MPU interface.
 */
int mpu_find_covering_region(void *addr, mpu_rasr_t *out);
bool mpu_instruction_executable(void *addr);

void mpu_disable_all_subregions(void);
void mpu_enable_subregion(void *addr);
void mpu_init(void);

#endif /* __MPU_H__ */

/*
 * mpu.h:
 *
 * Abstracted interface to handle configuration of the MPU.
 */

#ifndef __MPU_H__
#define __MPU_H__

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
void mpu_disable_all_subregions(void);
void mpu_enable_subregion(void *addr);
void mpu_init(void);

#endif /* __MPU_H__ */

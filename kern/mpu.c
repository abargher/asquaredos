#include "mpu.h"
#include "hardware/structs/mpu.h"

#include <stdbool.h>

/*
 * Note that these functions MUST be called from within an exception handler, or
 * immediately before it is expected that an exception would occur, in order to
 * enforce the memory barrier behavior described in the ARM documentation.
 */

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
    int enable          :1; /* Region enable bit. */
    int size            :5; /* Specified the size of the MPU region (2^SIZE). Maximum permitted value is 7. */
    int /* reserved. */ :2;
    int srd             :8; /* Subregion disable maps; 0 -> sr enabled; 1 -> sr disabled. */
    int bufferable      :1; /* Bufferable bit. */
    int cacheable       :1; /* Cacheable bit. */
    int shareable       :1; /* Shareable bit. */
    int /* reserved. */ :5;
    int ap              :3; /* Access permission field. */
    int /* reserved. */ :1;
    int xn              :1; /* Instruction access bit. */
    int /* reserved. */ :3;
} __attribute__((packed)) mpu_rasr_t;
volatile mpu_rasr_t *mpu_rasr = (mpu_rasr_t *)&mpu_hw->rasr;

/*
 * Layout of the MPU_RBAR register.
 */
typedef struct {
    int region          :4;     /* On writes, behavior depends on VALID field. On reads, returns current region number. */
    int valid           :1;     /* MPU region number valid bit; 0 = use MPU_RNR; 1 = set MPU_RNR to REGION field. */
    int /* reserved. */ :0;     /* Size of reserved region depends on region size. */
    int addr            :27;    /* Region base address field. Field size is variable w.r.t. region size; see ARM documentation. */
} __attribute__((packed)) mpu_rbar_t;
volatile mpu_rbar_t *mpu_rbar = (mpu_rbar_t *)&mpu_hw->rbar;

/*
 * Layout of the MPU_RNR register.
 */
typedef struct {
    int region          :8;     /* Indicates the MPU region referenced by MPU_RBAR and MPU_RASR registers. Permitted values are 0-7. */
    int /* reserved. */ :24;
} __attribute__((packed)) mpu_rnr_t;
volatile mpu_rnr_t *mpu_rnr = (mpu_rnr_t *)&mpu_hw->rnr;

/*
 * Layout of the MPU_CTRL register.
 */
typedef struct {
    int enable          :1;     /* Enables the MPU. */
    int hfnmiena        :1;     /* Enables the operation of MPU during HardFault and NMI handlers. */
    int privdefena      :1;     /* Enables privileged software access to the default memory map. */
    int /* reserved. */ :29;
} __attribute__((packed)) mpu_ctrl_t;
volatile mpu_ctrl_t *mpu_ctrl = (mpu_ctrl_t *)&mpu_hw->ctrl;

/*
 * Disable all subregions of all MPU regions.
 */
void
mpu_disable_all_subregions(void)
{
    /*
     * Iterate through all MPU regions and set all disabled bits.
     */
    for (int i = 0; i < MPU_NUM_REGIONS; i++) {
        mpu_rnr->region = i;
        mpu_rasr->srd = 0b11111111;
    }
}

/*
 * Enable the MPU subregion that protects the given address.
 */
void
mpu_enable_subregion(void *addr)
{
    if ((uintptr_t)addr < SRAM_BASE || (uintptr_t)addr >= SRAM_STRIPED_END) {
        panic("unable to disable non-existant region (addr = %p)", addr);
    }

    mpu_rnr->region = MPU_REGION(addr);
    mpu_rasr->srd &= ~(1 << MPU_SUBREGION(addr));
}

/*
 * Protect all subregions from all thread mode accesses, configure the MPU to
 * use the default memory map when no region is active, and enable the MPU.
 */
void
mpu_init(void)
{
    mpu_ctrl->hfnmiena = true;
    mpu_ctrl->privdefena = true;

    /*
     * Configure each region as R/W/X and enable the region, but disable all of
     * the subregions, causing the default memory map to be used for privileged
     * software, and all unprivileged accesses to generate a fault.
     */
    for (int i = 0; i < MPU_NUM_REGIONS; i++) {
        mpu_rbar->valid = true;
        mpu_rbar->region = i;
        mpu_rbar->addr = SRAM_BASE + (i << MPU_REGION_BITS);
        *mpu_rasr = (mpu_rasr_t){               /* Note: HW does not allow reserved fields to be overwritten, anyway. */
            .enable         = true,
            .size           = MPU_REGION_BITS,  /* 32 KB. */
            .srd            = 0b11111111,       /* All subregions disabled. */
            .bufferable     = true,
            .cacheable      = true,
            .shareable      = true,
            .ap             = MPU_AP__RW_RW,    /* Full access. */
            .xn             = 0,                /* Executable. */
        };
    }

    mpu_ctrl->enable = true;
}

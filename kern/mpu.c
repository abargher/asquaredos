#include "mpu.h"
#include "hardware/structs/mpu.h"

#include <stdbool.h>

/*
 * Note that these functions MUST be called from within an exception handler, or
 * immediately before it is expected that an exception would occur, in order to
 * enforce the memory barrier behavior described in the ARM documentation.
 */

/*
 * BEWARE! Do no try to write to the MPU registers via the bit fields defined in
 * these structs! The compiler will use single-byte instructions (e.g., strb)
 * which will NOT work. Instead, prepare an entire instance of the struct, and
 * write the entire register at once.
 * 
 * As a convience, use the SET_FIELD_SAFE macro below, which does this for you.
 * 
 * If you want to set more than one field at once, you NEED to cast your value
 * to something that isn't a struct, and use that to write the register...
 * 
 * Thank you GCC.
 */
#define MPU_FRIENDLY(struct_var) (*(io_rw_32 *)&struct_var)
#define SET_FIELD_SAFE(hwreg, type, field, value)                              \
    do {                                                                       \
        type temp = *(type *)&hwreg;                                           \
        temp.field = value;                                                    \
        hwreg = MPU_FRIENDLY(temp);                                            \
    } while (0)

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
    unsigned int size               :5; /* Specified the size of the MPU region (2^SIZE). Maximum permitted value is 7. */
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
volatile mpu_rasr_t *mpu_rasr = (mpu_rasr_t *)&mpu_hw->rasr;

/*
 * Layout of the MPU_RBAR register.
 */
typedef struct {
    unsigned int region             :4;     /* On writes, behavior depends on VALID field. On reads, returns current region number. */
    unsigned int valid              :1;     /* MPU region number valid bit; 0 = use MPU_RNR; 1 = set MPU_RNR to REGION field. */
    unsigned int /* reserved. */    :10;
    unsigned int addr               :17;    /* Region base address field. Bits [31:N], where N=log2(region size) = 15. */
} mpu_rbar_t;
volatile mpu_rbar_t *mpu_rbar = (mpu_rbar_t *)&mpu_hw->rbar;

/*
 * Layout of the MPU_RNR register.
 */
typedef struct {
    unsigned int region             :8;     /* Indicates the MPU region referenced by MPU_RBAR and MPU_RASR registers. Permitted values are 0-7. */
    unsigned int /* reserved. */    :24;
} mpu_rnr_t;
volatile mpu_rnr_t *mpu_rnr = (mpu_rnr_t *)&mpu_hw->rnr;

/*
 * Layout of the MPU_CTRL register.
 */
typedef struct {
    unsigned int enable             :1;     /* Enables the MPU. */
    unsigned int hfnmiena           :1;     /* Enables the operation of MPU during HardFault and NMI handlers. */
    unsigned int privdefena         :1;     /* Enables privileged software access to the default memory map. */
    unsigned int /* reserved. */    :29;
} mpu_ctrl_t;
volatile mpu_ctrl_t *mpu_ctrl = (mpu_ctrl_t *)&mpu_hw->ctrl;


#define MPU_CTRL_SET_ENABLE(enable) SET_BITS_OF_WORD(mpu_hw->ctrl, 0, 1, enable)

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
        SET_FIELD_SAFE(mpu_hw->rnr, mpu_rnr_t, region, i);
        SET_FIELD_SAFE(mpu_hw->rasr, mpu_rasr_t, srd, 0b11111111);
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

    SET_FIELD_SAFE(mpu_hw->rnr, mpu_rnr_t, region, MPU_REGION(addr));
    SET_FIELD_SAFE(mpu_hw->rasr, mpu_rasr_t, srd, mpu_rasr->srd & ~(1 << MPU_SUBREGION(addr)));
}

void
foo(volatile void *x)
{
    printf("%p", x);
}

/*
 * Protect all subregions from all thread mode accesses, configure the MPU to
 * use the default memory map when no region is active, and enable the MPU.
 */
void
mpu_init(void)
{
    /* solely for debugging so the compiler doesn't optimize these away. */
    volatile void *k = mpu_rnr; foo(k);
    volatile void *x = mpu_rbar; foo(x);
    volatile void *y = mpu_rasr; foo(y);
    volatile void *z = mpu_ctrl; foo(z);

    /*
     * Enable HardFaults to occur in the HardFault and NMI handlers. This
     * behaviour should not occur if things are written properly; it'd be
     * better to know sooner via a hardfault than later by a long debugging
     * session.
     */

    /*
     * Configure each region as R/W/X and enable the region, but disable all of
     * the subregions, causing the default memory map to be used for privileged
     * software, and all unprivileged accesses to generate a fault.
     */
    for (int i = 0; i < MPU_NUM_REGIONS; i++) {
        mpu_rbar_t rbar = {
            .addr       = (SRAM_BASE + i * MPU_REGION_SIZE) >> MPU_REGION_BITS,
            .region     = i,
            .valid      = true
        };
        mpu_hw->rbar = MPU_FRIENDLY(rbar);

        mpu_rasr_t rasr = {               /* Note: HW does not allow reserved fields to be overwritten, anyway. */
            .enable         = true,
            .size           = MPU_REGION_BITS - 1,  /* 32 KB. */
            .srd            = 0b11111111,       /* All subregions disabled. */
            .bufferable     = 0,
            .cacheable      = 1,
            .shareable      = 0,
            .ap             = MPU_AP__RW_RW,    /* Full access. */
            .xn             = 1                 /* Executable. */
        };
        mpu_hw->rasr = MPU_FRIENDLY(rasr);
    }

    /*
     * Enable the MPU.
     */
    mpu_ctrl_t ctrl = {
        .enable = true,
        .hfnmiena = true,
        .privdefena = true
    };
    mpu_hw->ctrl = MPU_FRIENDLY(ctrl);
}

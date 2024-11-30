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
 */
#define MPU_FRIENDLY(struct_var) (*(io_rw_32 *)&struct_var)
#define SET_FIELD_SAFE(hwreg, type, field, value)                              \
    do {                                                                       \
        type temp = *(type *)&hwreg;                                           \
        temp.field = value;                                                    \
        hwreg = MPU_FRIENDLY(temp);                                            \
    } while (0)

#define GET_FIELD_SAFE(hwreg, type, field, out)                                \
    do {                                                                       \
        type temp = *(type *)&hwreg;                                           \
        out = temp.field;                                                      \
    } while (0)

/*
 * Properly configure the MPU_RBAR register for the given region and base
 * address.
 */
#define SET_RBAR(base, region)                                                 \
    do {                                                                       \
        mpu_hw->rbar = (base) & (~0x1F) | (1 << 4) | (region);                 \
    } while (0)

/*
 * Get the base address of the currently selected region.
 */
#define GET_RBAR() ((void *)(mpu_hw->rbar & (~0x1F)))

#define FIRST_VM_SUBREGION (2)

/*
 * Read-only convenience aliases to make reading the registers easier.
 */
const mpu_rasr_t   *mpu_rasr    = (mpu_rasr_t *)&mpu_hw->rasr;
const mpu_rbar_t   *mpu_rbar    = (mpu_rbar_t *)&mpu_hw->rbar;
const mpu_rnr_t    *mpu_rnr     = (mpu_rnr_t *)&mpu_hw->rnr;
const mpu_ctrl_t   *mpu_ctrl    = (mpu_ctrl_t *)&mpu_hw->ctrl;

/*
 * If the provided address is protected by an active region, returns the number
 * of that region and writes the contents of that region's RASR register to the
 * provided pointer. Otherwise, returns -1.
 */
int
mpu_find_covering_region(void *addr, mpu_rasr_t *rasr_out)
{
    /*
     * Start from the highest numbered region, because higher numbered regions
     * have greater priority if the address is covered by multiple regions.
     */
    for (int i = MPU_NUM_REGIONS - 1; i >= 0; i--) {
        SET_FIELD_SAFE(mpu_hw->rnr, mpu_rnr_t, region, i);
        *rasr_out = *mpu_rasr;
        void *region_base = GET_RBAR();

        if ((addr < region_base) || (addr >= region_base + (1 << (1 + rasr_out->size)))) {
            /*
             * Address out of range.
             */
            continue;
        }

        if (!rasr_out->enable || (rasr_out->srd & (1 << MPU_SUBREGION(addr)))) {
            /*
             * Region or subregion is disabled.
             */
            continue;
        }

        /*
         * This MPU region covers the address, is enabled, and the subregion
         * covering this address is also enabled.
         */
        return i;
    }

    return -1;
}

/*
 * Returns true if the address is covered by an executable MPU region, false
 * if not.
 */
bool
mpu_instruction_executable(void *addr)
{
    mpu_rasr_t rasr;
    int region = mpu_find_covering_region(addr, &rasr);
    if (region < 0) {
        return false;
    }

    return !rasr.xn;
}

/*
 * Disable all subregions of all MPU regions.
 */
void
mpu_disable_all_subregions(void)
{
    /*
     * Iterate through all MPU regions and set all disabled bits.
     */
    for (int i = FIRST_VM_SUBREGION; i < MPU_NUM_REGIONS; i++) {
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

    unsigned int srd;
    SET_FIELD_SAFE(mpu_hw->rnr, mpu_rnr_t, region, MPU_REGION(addr));
    GET_FIELD_SAFE(mpu_hw->rasr, mpu_rasr_t, srd, srd);
    SET_FIELD_SAFE(mpu_hw->rasr, mpu_rasr_t, srd, srd & ~(1 << MPU_SUBREGION(addr)));
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
     * Configure each region as R/W/X and enable the region, but disable all of
     * the subregions, causing the default memory map to be used for privileged
     * software, and all unprivileged accesses to generate a fault.
     * 
     * Note we begin at i=2 because each region is 32KB, and the 64KB write
     * cache does not need fine-grained protection.
     */
    mpu_rasr_t rasr;
    for (int i = FIRST_VM_SUBREGION; i < MPU_NUM_REGIONS; i++) {
        SET_RBAR(SRAM_BASE + i * MPU_REGION_SIZE, i);

        rasr = (mpu_rasr_t){
            .enable         = 1,
            .size           = MPU_REGION_BITS - 1,  /* 32 KB. */
            .srd            = 0b11111111,       /* All subregions disabled. */
            .bufferable     = 0,
            .cacheable      = 1,
            .shareable      = 0,
            .ap             = MPU_AP__RW_RW,    /* Full access. */
            .xn             = 0                 /* Executable. */
        };
        mpu_hw->rasr = MPU_FRIENDLY(rasr);
    }

    /*
     * Repurpose the two unused regions (0 and 1) to write-protect flash, and
     * provide a no-read, no-write, no-execute background region for SRAM.
     */

    /*
     * SRAM background region.
     */
    SET_RBAR(SRAM_BASE, 0);
    rasr = (mpu_rasr_t) {
        .enable         = 1,
        .size           = 17,               /* 2 << 17 == 256KB. */
        .srd            = 0b00000000,
        .bufferable     = 0,
        .cacheable      = 1,
        .shareable      = 0,
        .ap             = MPU_AP__RW_NA,    /* Privileged access only. */
        .xn             = 1
    };
    mpu_hw->rasr = MPU_FRIENDLY(rasr);

    /*
     * Enable HardFaults to occur in the HardFault and NMI handlers. This
     * behaviour should not occur if things are written properly; it'd be
     * better to know sooner via a hardfault than later by a long debugging
     * session.
     */
    mpu_ctrl_t ctrl = {
        .enable = true,
        .hfnmiena = false,
        .privdefena = true
    };
    mpu_hw->ctrl = MPU_FRIENDLY(ctrl);
}

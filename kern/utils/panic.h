/*
 * panic.h:
 *
 * Offers several panic macros that causes the system to enter debug mode when
 * triggered.
 */

#ifndef __PANIC_H__
#define __PANIC_H__
#include "pico/stdlib.h"

#define LED_BUILTIN 25

/*
 * Illuminates the LED and puts the RP2040 into debug mode. See section 2.3.4 in
 * the datasheet (page 61) for details about how to debug a device in this mode.
 */
#define panic()                                                                \
    do {                                                                       \
        /* gpio_put(LED_BUILTIN, 1);   Illuminate the LED. */                  \
        /* asm("bkpt #0");             Put us in debug mode. */                \
    } while (0)

#undef assert
#define assert(condition)                                                      \
    do {                                                                       \
        if (condition) {                                                       \
            panic();                                                           \
        }                                                                      \
    } while (0)

#endif /* __PANIC_H__ */

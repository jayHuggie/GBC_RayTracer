/**
 * util.h - GBC utility macros
 * 
 * Common utility macros for Game Boy Color development.
 */

#ifndef _UTIL_H
#define _UTIL_H

#include <gb/gb.h>
#include <stdint.h>

/**
 * Disable interrupts and turn off the LCD.
 * Must be done before bulk VRAM writes during initialization.
 */
#define lcd_off() do { \
    disable_interrupts(); \
    DISPLAY_OFF; \
} while (0)

/**
 * Turn the LCD on and enable interrupts.
 * Call after VRAM initialization is complete.
 */
#define lcd_on() do { \
    DISPLAY_ON; \
    enable_interrupts(); \
} while (0)

#endif /* _UTIL_H */

/**
 * graphics.h - GBC graphics and palette definitions
 * 
 * Provides palette setup for the raytracer display.
 */

#ifndef _GRAPHICS_H
#define _GRAPHICS_H

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

/*============================================================================
 * FUNCTION DECLARATIONS
 *============================================================================*/

/**
 * Load palettes into GBC palette RAM.
 * Must be called during initialization.
 */
void load_palettes(void);

/**
 * Set up palette attributes for render area vs border.
 * Must be called after raytracer_init() with LCD off.
 */
void setup_palette_attributes(void);

#endif /* _GRAPHICS_H */

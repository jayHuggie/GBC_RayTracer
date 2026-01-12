/**
 * graphics.h - GBC graphics and palette definitions
 */

#ifndef _GRAPHICS_H
#define _GRAPHICS_H

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

void load_palettes(void);
void setup_palette_attributes(void);

#endif

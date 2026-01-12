/**
 * graphics.c - GBC palette setup for raytracer
 * 
 * COLORFUL PALETTE DESIGN:
 * Since GBC allows only 4 colors per palette, we map materials to colors:
 *   - Shade 0: Dark shadow (universal)
 *   - Shade 1: Red (sphere)
 *   - Shade 2: Green (ground)
 *   - Shade 3: Sky blue (background/highlights)
 */

#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "graphics.h"
#include "raytracer.h"

/*============================================================================
 * COLORFUL PALETTES
 *============================================================================*/

/* Main colorful palette for the scene */
static const uint16_t raytracer_palette[] = {
    RGB8(24,  16,  32),    /* 0: Dark purple-ish shadow */
    RGB8(220, 60,  60),    /* 1: Bright red (sphere) */
    RGB8(60,  180, 80),    /* 2: Green (ground) */
    RGB8(135, 206, 235)    /* 3: Sky blue */
};

/* Border palette - darker complementary */
static const uint16_t border_palette[] = {
    RGB8(8,   8,   16),    /* 0: Very dark */
    RGB8(16,  16,  32),    /* 1: Dark blue */
    RGB8(24,  24,  48),    /* 2: Medium dark blue */
    RGB8(40,  40,  80)     /* 3: Border fill color */
};

void load_palettes(void) {
    set_bkg_palette(0, 1, raytracer_palette);
    set_bkg_palette(1, 1, border_palette);
}

void setup_palette_attributes(void) {
    uint8_t map_offset_x = RENDER_OFFSET_X / 8;
    uint8_t map_offset_y = RENDER_OFFSET_Y / 8;
    
    /* Switch to attribute layer */
    VBK_REG = VBK_ATTRIBUTES;
    
    /* Fill entire visible area with border palette (1) */
    uint8_t border_attr[32];
    memset(border_attr, 1, 32);
    for (uint8_t y = 0; y < 18; y++) {
        set_bkg_tiles(0, y, 32, 1, border_attr);
    }
    
    /* Set render area to use palette 0 */
    uint8_t render_attr[RENDER_TILES_X];
    memset(render_attr, 0, RENDER_TILES_X);
    for (uint8_t y = 0; y < RENDER_TILES_Y; y++) {
        set_bkg_tiles(map_offset_x, map_offset_y + y, RENDER_TILES_X, 1, render_attr);
    }
    
    /* Switch back to tile layer */
    VBK_REG = VBK_TILES;
}

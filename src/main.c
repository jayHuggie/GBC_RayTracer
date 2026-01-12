/**
 * main.c - Game Boy Color Raytracer Entry Point
 * 
 * Progressive rendering: renders one tile row at a time with vsync()
 * between rows so the display stays responsive and you can watch
 * the image build up.
 */

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>

#include "graphics.h"
#include "raytracer.h"
#include "util.h"

void main(void) {
    /*========================================================================
     * INITIALIZATION
     *========================================================================*/
    
    lcd_off();
    
    /* Configure LCD: background on, use tile data at 0x8000 */
    LCDC_REG = LCDCF_OFF | LCDCF_BGON | LCDCF_BG8000;
    
    /* Load color palettes */
    load_palettes();
    
    /* Initialize VRAM tile map and border */
    raytracer_init_vram();
    
    /* Set palette attributes (which palette for each tile) */
    setup_palette_attributes();
    
    /* Turn LCD on - user will see black render area with border */
    lcd_on();
    
    /*========================================================================
     * PROGRESSIVE RENDERING
     * 
     * Render one row of tiles at a time, uploading to VRAM after each.
     * This lets the screen stay responsive and shows the image building up.
     *========================================================================*/
    
    for (uint8_t row = 0; row < RENDER_TILES_Y; row++) {
        /* Render this tile row (computationally expensive) */
        raytracer_render_row(row);
        
        /* Wait for VBlank, then upload the rendered row */
        vsync();
        raytracer_upload_row(row);
    }
    
    /*========================================================================
     * MAIN LOOP (idle - scene is complete)
     *========================================================================*/
    
    while (1) {
        vsync();
    }
}

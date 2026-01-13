/**
 * main.c - 2-View Gallery Raytracer
 * 
 * Pre-renders front and back views with visual progress bar.
 * D-pad Up/Down switches between views.
 */

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>
#include <string.h>

#include "graphics.h"
#include "raytracer.h"
#include "util.h"

/*============================================================================
 * PROGRESS BAR
 * 
 * Shows rendering progress at top of screen using colored tiles.
 * Uses tile 145 (after render tiles 1-144).
 *============================================================================*/

#define TILE_BORDER     0
#define TILE_PROGRESS   (RENDER_TILE_BASE + MAX_RENDER_TILES)  /* 145 */
#define PROGRESS_Y      0
#define PROGRESS_WIDTH  20

static void init_progress_tile(void) {
    /* Create progress tile - solid color 2 (green) */
    uint8_t progress_tile[16] = {
        0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
        0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF
    };  /* This makes color index 2 (green ground color) */
    set_bkg_data(TILE_PROGRESS, 1, progress_tile);
}

static void show_progress_scanline(uint8_t view, uint8_t scanline) {
    /* Progress: view * 96 + scanline out of 192 total scanlines */
    uint16_t total = NUM_VIEWS * RENDER_HEIGHT;
    uint16_t current = view * RENDER_HEIGHT + scanline;
    uint8_t filled = (uint8_t)((current * PROGRESS_WIDTH) / total);
    
    uint8_t bar[PROGRESS_WIDTH];
    for (uint8_t i = 0; i < PROGRESS_WIDTH; i++) {
        bar[i] = (i < filled) ? TILE_PROGRESS : TILE_BORDER;
    }
    set_bkg_tiles(0, PROGRESS_Y, PROGRESS_WIDTH, 1, bar);
}

static void clear_progress(void) {
    uint8_t bar[PROGRESS_WIDTH];
    for (uint8_t i = 0; i < PROGRESS_WIDTH; i++) {
        bar[i] = TILE_BORDER;
    }
    set_bkg_tiles(0, PROGRESS_Y, PROGRESS_WIDTH, 1, bar);
}

static void clear_render_area(void) {
    /* Clear all render tiles to empty/black */
    uint8_t empty_tile[16];
    memset(empty_tile, 0x00, 16);
    
    for (uint8_t i = 0; i < MAX_RENDER_TILES; i++) {
        set_bkg_data(RENDER_TILE_BASE + i, 1, empty_tile);
    }
}

/*============================================================================
 * RENDER ONE VIEW (SCANLINE-BY-SCANLINE for smooth visual feedback)
 *============================================================================*/

static void render_view(uint8_t view_id) {
    raytracer_set_view(view_id);
    
    /* Render 96 scanlines, one at a time */
    for (uint8_t py = 0; py < RENDER_HEIGHT; py++) {
        /* Render single scanline */
        raytracer_render_scanline(py);
        
        /* Update progress bar */
        show_progress_scanline(view_id, py + 1);
        
        /* Wait for VBlank and upload scanline */
        wait_vbl_done();
        raytracer_upload_scanline(py);
        
        /* When we complete a tile row (every 8 scanlines), store it */
        if ((py & 7) == 7) {
            uint8_t tile_row = py / 8;
            raytracer_store_row(view_id, tile_row);
        }
    }
}

/*============================================================================
 * MAIN
 *============================================================================*/

void main(void) {
    uint8_t current_view = VIEW_FRONT;
    uint8_t last_keys = 0;
    
    DISPLAY_OFF;
    LCDC_REG = LCDCF_OFF | LCDCF_BGON | LCDCF_BG8000;
    
    load_palettes();
    raytracer_init_vram();
    setup_palette_attributes();
    init_progress_tile();
    
    /* Initialize raytracer LUTs and precomputed arrays */
    raytracer_init();
    
    DISPLAY_ON;
    
    /* Pre-render both views */
    render_view(VIEW_FRONT);
    
    /* Wipe screen before rendering second view */
    wait_vbl_done();
    clear_render_area();
    
    render_view(VIEW_BACK);
    
    clear_progress();
    
    /* Load front view */
    current_view = VIEW_FRONT;
    wait_vbl_done();
    raytracer_load_scene(current_view);
    
    /* Main loop */
    while (1) {
        wait_vbl_done();
        
        uint8_t keys = joypad();
        uint8_t pressed = keys & ~last_keys;
        last_keys = keys;
        
        uint8_t new_view = current_view;
        
        if (pressed & J_DOWN) new_view = VIEW_FRONT;
        if (pressed & J_UP)   new_view = VIEW_BACK;
        
        if (new_view != current_view) {
            current_view = new_view;
            raytracer_load_scene(current_view);
        }
    }
}

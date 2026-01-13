/**
 * main.c - 2-View Gallery Raytracer with Title Screen
 * 
 * Shows title screen, then pre-renders front and back views.
 * D-pad Up/Down switches between views.
 */

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>
#include <string.h>

#include "graphics.h"
#include "raytracer.h"
#include "util.h"
#include "TitleScreen.h"

/*============================================================================
 * TITLE SCREEN
 * 
 * Shows the title screen and waits for START or A button.
 *============================================================================*/

static void show_title_screen(void) {
    DISPLAY_OFF;
    
    /* Load title screen tiles (starting at tile 0) */
    set_bkg_data(0, TitleScreen_TILE_COUNT, TitleScreen_tiles);
    
    /* Load title screen tilemap (20x18 tiles = full screen) */
    set_bkg_tiles(0, 0, 20, 18, TitleScreen_map);
    
    /* Load BOOSTED title screen palettes for more vibrant colors */
    /* Original colors were washed out due to GBC's 15-bit color limit */
    static const palette_color_t boosted_palettes[12] = {
        /* Palette 0: white, teal, RED (C), black */
        RGB8(255,255,255), RGB8( 55,148,110), RGB8(220, 40, 40), RGB8(  0,  0,  0),
        /* Palette 1: white, YELLOW (o), GREEN (L), PURPLE (o) */
        RGB8(255,255,255), RGB8(255,248, 40), RGB8(140,240, 70), RGB8(130, 70,160),
        /* Palette 2: white, light blue, DARK BLUE (R), black */
        RGB8(255,255,255), RGB8(100,160,255), RGB8( 50,110,160), RGB8(  0,  0,  0)
    };
    set_bkg_palette(0, TitleScreen_PALETTE_COUNT, boosted_palettes);
    
    /* Set palette attributes for multi-color "COLOR" text */
    VBK_REG = 1;  /* Switch to VRAM bank 1 (attributes) */
    uint8_t attr_map[20 * 18];
    
    /* Default: use palette 0 for all tiles (black text, red for C) */
    for (uint16_t i = 0; i < 20 * 18; i++) {
        attr_map[i] = 0;
    }
    
    /* "CoLoR" appears on rows 9-11 (below "Game Boy" which is rows 5-7)
     * 
     * Colors from user's indexed PNG:
     * C=#ac3232 (red)    → Palette 0 (index 2)
     * o=#76428a (purple) → Palette 1 (index 3)
     * L=#99e550 (green)  → Palette 1 (index 2)
     * o=#fbf236 (yellow) → Palette 1 (index 1)
     * R=#306082 (blue)   → Palette 2 (index 2)
     * 
     * Estimated columns: C=5-6, o=7-8, L=9-10, o=11-12, R=13-14
     */
    
    for (uint8_t row = 9; row <= 11; row++) {
        /* C (cols 5-6) - palette 0 (red) - already default */
        attr_map[row * 20 + 7]  = 1;  /* o - purple (palette 1) */
        attr_map[row * 20 + 8]  = 1;  /* o continued */
        attr_map[row * 20 + 9]  = 1;  /* L - green (palette 1) */
        attr_map[row * 20 + 10] = 1;  /* L continued */
        attr_map[row * 20 + 11] = 1;  /* o - yellow (palette 1) */
        attr_map[row * 20 + 12] = 1;  /* o continued */
        attr_map[row * 20 + 13] = 2;  /* R - blue (palette 2) */
        attr_map[row * 20 + 14] = 2;  /* R continued */
    }
    
    set_bkg_tiles(0, 0, 20, 18, attr_map);
    VBK_REG = 0;  /* Switch back to VRAM bank 0 */
    
    DISPLAY_ON;
}

static void wait_for_start(void) {
    uint8_t last_keys = joypad();
    
    while (1) {
        wait_vbl_done();
        
        uint8_t keys = joypad();
        uint8_t pressed = keys & ~last_keys;
        last_keys = keys;
        
        /* Wait for START or A button */
        if (pressed & (J_START | J_A)) {
            break;
        }
    }
}

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
    
    /* === TITLE SCREEN === */
    LCDC_REG = LCDCF_OFF | LCDCF_BGON | LCDCF_BG8000;
    show_title_screen();
    wait_for_start();
    
    /* === TRANSITION TO RAYTRACER === */
    DISPLAY_OFF;
    
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

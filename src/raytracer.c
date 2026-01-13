/**
 * raytracer.c - 2-view gallery raytracer with smooth dithered shading
 * 
 * OPTIMIZED VERSION:
 * (1) LUTs eliminate per-pixel divisions for sphere intersection
 * (2) Ground intersection precomputed per scanline
 * (3) dx/dy arrays precomputed to avoid repeated calculations
 */

#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "raytracer.h"

/*============================================================================
 * SCENE STORAGE (2 views × 2304 bytes = 4608 bytes)
 *============================================================================*/

static uint8_t scene_buffer[NUM_VIEWS][SCENE_SIZE];
static uint8_t tile_row_buffer[RENDER_TILES_X * 16];

/* Current view affects light direction for back view */
static int8_t light_dir_x = -1;  /* -1 for front, +1 for back */
static int8_t light_dir_z = 1;   /* +1 for front, -1 for back */

/*============================================================================
 * OPTIMIZATION 1: LOOKUP TABLES FOR SPHERE INTERSECTION
 * d_dot_d ranges from ~256 to ~706 based on screen coordinates
 * Use 64-entry quantized LUT to save memory (GBC only has 8KB WRAM!)
 *============================================================================*/

#define LUT_MIN_VAL   256
#define LUT_MAX_VAL   768
#define LUT_SHIFT     3       /* Divide d_dot_d range by 8 */
#define LUT_SIZE      64      /* (768-256)/8 = 64 entries */

/* LUT for t_hit = (oc_dot_d << FX8_SHIFT) / d_dot_d */
static int16_t lut_t_hit[LUT_SIZE];

/* LUT for proj_sq = (oc_dot_d * oc_dot_d) / d_dot_d */
static int16_t lut_proj_sq[LUT_SIZE];  /* Use int16_t to save memory */

/* oc_dot_d is constant because oc_y_fp = 0 (cam and sphere same Y) */
static int16_t oc_dot_d_constant;

/*============================================================================
 * OPTIMIZATION 2: PER-SCANLINE PRECOMPUTED GROUND INTERSECTION
 *============================================================================*/

static int16_t scanline_dy_fp[RENDER_HEIGHT];
static int16_t scanline_t_ground[RENDER_HEIGHT];
static uint8_t scanline_hit_ground[RENDER_HEIGHT];

/*============================================================================
 * OPTIMIZATION 3: PRECOMPUTED DX/DY ARRAYS
 *============================================================================*/

static int16_t dx_fp_array[RENDER_WIDTH];
static int32_t dx_sq_array[RENDER_WIDTH];
static int16_t dy_fp_array[RENDER_HEIGHT];
static int32_t dy_sq_array[RENDER_HEIGHT];
static int32_t dz_sq_constant;

/*============================================================================
 * OPTIMIZATION 4: SHADOW BRIGHTNESS LUT
 * Maps shadow_dist_sq directly to brightness (0-255) with no division!
 * Range: 0 to ~1200 (sphere_radius_sq << 8), quantized to 128 entries
 *============================================================================*/

#define SHADOW_LUT_SIZE   128
#define SHADOW_LUT_SHIFT  3     /* Divide shadow_dist_sq by 8 for indexing */

static uint8_t shadow_brightness_lut[SHADOW_LUT_SIZE];

/*============================================================================
 * OPTIMIZATION 5: SHADOW CONSTANTS (precomputed per view)
 * Removes ALL per-pixel divisions from shadow calculation!
 *============================================================================*/

static int16_t shadow_center_x_const;  /* Shadow center X (view-dependent) */
static int16_t shadow_center_z_const;  /* Shadow center Z (view-dependent) */

/*============================================================================
 * OPTIMIZATION 6: PER-SCANLINE SHADOW TERMS
 * shadow_dz only depends on py, so precompute per scanline
 *============================================================================*/

static int16_t scanline_ground_z[RENDER_HEIGHT];      /* ground_z for each scanline */
static int32_t scanline_shadow_dz_sq[RENDER_HEIGHT];  /* (ground_z - shadow_center_z)^2 */

/*============================================================================
 * DITHERING (2x2 Bayer matrix)
 *============================================================================*/

static const uint8_t bayer2x2[2][2] = {
    {  0, 128 },
    { 192,  64 }
};

static uint8_t dither(uint8_t brightness, uint8_t dark_color, uint8_t bright_color, 
                      uint8_t px, uint8_t py) {
    uint8_t threshold = bayer2x2[py & 1][px & 1];
    return (brightness > threshold) ? bright_color : dark_color;
}

/*============================================================================
 * INITIALIZATION: BUILD LUTs AND PRECOMPUTED ARRAYS
 *============================================================================*/

static void init_luts(void) {
    /* oc_dot_d is constant: oc_y_fp=0, oc_z_fp=SPHERE_CZ<<8, dz_fp=256 */
    int16_t oc_z_fp = SPHERE_CZ << FX8_SHIFT;
    int16_t dz_fp = FX8_ONE;
    oc_dot_d_constant = (int16_t)(((int32_t)oc_z_fp * dz_fp) >> FX8_SHIFT);  /* 1536 */
    
    /* Build quantized LUTs for sphere intersection divisions */
    for (uint8_t i = 0; i < LUT_SIZE; i++) {
        /* d_dot_d value for this LUT entry (center of quantized range) */
        int32_t d_dot_d = LUT_MIN_VAL + ((int32_t)i << LUT_SHIFT) + (1 << (LUT_SHIFT - 1));
        
        /* t_hit = (oc_dot_d << FX8_SHIFT) / d_dot_d */
        lut_t_hit[i] = (int16_t)(((int32_t)oc_dot_d_constant << FX8_SHIFT) / d_dot_d);
        
        /* proj_sq = (oc_dot_d * oc_dot_d) / d_dot_d (scaled down to fit int16) */
        lut_proj_sq[i] = (int16_t)(((int32_t)oc_dot_d_constant * oc_dot_d_constant) / d_dot_d);
    }
}

static void init_shadow_lut(void) {
    /* Shadow parameters (matching trace_ray calculations) */
    int16_t shadow_radius_sq = (int16_t)(SPHERE_R_SQ << FX8_SHIFT);      /* 1024 */
    int16_t umbra_radius_sq = shadow_radius_sq >> 2;                     /* 256 */
    int16_t penumbra_range = shadow_radius_sq - umbra_radius_sq;         /* 768 */
    
    /* Build LUT: map shadow_dist_sq to brightness (0-255) */
    for (uint8_t i = 0; i < SHADOW_LUT_SIZE; i++) {
        /* shadow_dist_sq value for this LUT entry */
        int16_t dist_sq = (int16_t)((int32_t)i << SHADOW_LUT_SHIFT);
        
        if (dist_sq >= shadow_radius_sq) {
            /* Outside shadow */
            shadow_brightness_lut[i] = 255;
        } else if (dist_sq <= umbra_radius_sq) {
            /* Full shadow (umbra) */
            shadow_brightness_lut[i] = 0;
        } else {
            /* Penumbra (soft edge) - linear interpolation */
            int16_t dist_in_penumbra = dist_sq - umbra_radius_sq;
            uint8_t brightness = (uint8_t)(((int32_t)dist_in_penumbra * 256) / penumbra_range);
            shadow_brightness_lut[i] = brightness;
        }
    }
}

static void init_dx_dy_arrays(void) {
    int16_t half_w = RENDER_WIDTH / 2;
    int16_t half_h = RENDER_HEIGHT / 2;
    
    /* Precompute dx_fp and dx_sq for each x coordinate */
    for (uint8_t x = 0; x < RENDER_WIDTH; x++) {
        int16_t dx = ((int16_t)x - half_w) * 5;
        dx_fp_array[x] = dx;
        dx_sq_array[x] = (int32_t)dx * dx;
    }
    
    /* Precompute dy_fp and dy_sq for each y coordinate */
    for (uint8_t y = 0; y < RENDER_HEIGHT; y++) {
        int16_t dy = (half_h - (int16_t)y) * 5;
        dy_fp_array[y] = dy;
        dy_sq_array[y] = (int32_t)dy * dy;
    }
    
    /* dz is constant */
    int16_t dz_fp = FX8_ONE;
    dz_sq_constant = (int32_t)dz_fp * dz_fp;
}

static void init_ground_scanlines(void) {
    int16_t dz_fp = FX8_ONE;
    
    /* Precompute ground intersection for each scanline */
    for (uint8_t py = 0; py < RENDER_HEIGHT; py++) {
        int16_t dy_fp = dy_fp_array[py];
        scanline_dy_fp[py] = dy_fp;
        
        if (dy_fp < -16) {
            /* t_ground = (-CAM_Y << (FX8_SHIFT + FX8_SHIFT)) / dy_fp */
            int16_t t_ground = (int16_t)(((int32_t)(-CAM_Y) << (FX8_SHIFT + FX8_SHIFT)) / dy_fp);
            
            if (t_ground > 16 && t_ground < 2000) {
                scanline_hit_ground[py] = 1;
                scanline_t_ground[py] = t_ground;
                /* OPTIMIZATION 6: Precompute ground_z for shadow calculation */
                scanline_ground_z[py] = (int16_t)(((int32_t)dz_fp * t_ground) >> FX8_SHIFT);
            } else {
                scanline_hit_ground[py] = 0;
                scanline_t_ground[py] = 0;
                scanline_ground_z[py] = 0;
            }
        } else {
            scanline_hit_ground[py] = 0;
            scanline_t_ground[py] = 0;
            scanline_ground_z[py] = 0;
        }
    }
}

static void init_shadow_scanlines(void) {
    /* OPTIMIZATION 6: Precompute shadow_dz_sq for each scanline */
    for (uint8_t py = 0; py < RENDER_HEIGHT; py++) {
        if (scanline_hit_ground[py]) {
            int32_t shadow_dz = (int32_t)scanline_ground_z[py] - shadow_center_z_const;
            scanline_shadow_dz_sq[py] = (shadow_dz * shadow_dz) >> FX8_SHIFT;
        } else {
            scanline_shadow_dz_sq[py] = 0;
        }
    }
}

/*============================================================================
 * CAMERA SETUP
 *============================================================================*/

void raytracer_set_view(uint8_t view_id) {
    if (view_id == VIEW_FRONT) {
        /* Light from upper-left-front */
        light_dir_x = -1;
        light_dir_z = 1;
    } else {
        /* Back view: flip light X direction */
        light_dir_x = 1;
        light_dir_z = 1;
    }
    
    /* OPTIMIZATION 5: Precompute shadow constants (removes per-pixel division!) */
    /* t_shadow = (SPHERE_CY << 16) / LIGHT_Y -- computed ONCE per view */
    int32_t t_shadow = ((int32_t)SPHERE_CY << 16) / LIGHT_Y;
    
    /* shadow_center = sphere_pos + (-light_dir * t_shadow) */
    shadow_center_x_const = (int16_t)((SPHERE_CX << FX8_SHIFT) + 
                            (((-light_dir_x * LIGHT_X) * t_shadow) >> FX8_SHIFT));
    shadow_center_z_const = (int16_t)((SPHERE_CZ << FX8_SHIFT) + 
                            (((-light_dir_z * LIGHT_Z) * t_shadow) >> FX8_SHIFT));
    
    /* Rebuild scanline LUTs */
    init_ground_scanlines();
    
    /* OPTIMIZATION 6: Rebuild per-scanline shadow terms */
    init_shadow_scanlines();
}

/*============================================================================
 * RAY TRACING (OPTIMIZED)
 *============================================================================*/

static uint8_t trace_ray(uint8_t px, uint8_t py) {
    /* OPTIMIZATION 3: Use precomputed dx/dy arrays */
    int16_t dx_fp = dx_fp_array[px];
    int16_t dy_fp = dy_fp_array[py];
    int16_t dz_fp = FX8_ONE;
    
    /* OPTIMIZATION 3: d_dot_d from precomputed squares */
    int16_t d_dot_d = (int16_t)((dx_sq_array[px] + dy_sq_array[py] + dz_sq_constant) >> FX8_SHIFT);
    
    /* Clamp d_dot_d to LUT range and compute quantized index */
    if (d_dot_d < LUT_MIN_VAL) d_dot_d = LUT_MIN_VAL;
    if (d_dot_d > LUT_MAX_VAL) d_dot_d = LUT_MAX_VAL;
    
    /* OPTIMIZATION 1: Use quantized LUTs for sphere intersection (no division!) */
    uint8_t lut_index = (uint8_t)((d_dot_d - LUT_MIN_VAL) >> LUT_SHIFT);
    if (lut_index >= LUT_SIZE) lut_index = LUT_SIZE - 1;
    
    int16_t t_hit = lut_t_hit[lut_index];
    int32_t proj_sq = (int32_t)lut_proj_sq[lut_index];
    
    /* Sphere intersection test */
    int32_t oc_sq = SPHERE_CZ * SPHERE_CZ;
    int32_t dist_sq_fp = (oc_sq << FX8_SHIFT) - proj_sq;
    int32_t radius_sq_fp = SPHERE_R_SQ << FX8_SHIFT;
    
    uint8_t hit_sphere = 0;
    
    if (dist_sq_fp < radius_sq_fp && oc_dot_d_constant > 0) {
        hit_sphere = 1;
        /* t_hit already loaded from LUT */
    }
    
    /* OPTIMIZATION 2: Ground intersection from scanline precompute (no division!) */
    uint8_t hit_ground = scanline_hit_ground[py];
    int16_t t_ground = scanline_t_ground[py];
    int16_t ground_x = 0, ground_z = 0;
    
    if (hit_ground) {
        ground_x = (int16_t)(((int32_t)dx_fp * t_ground) >> FX8_SHIFT);
        ground_z = (int16_t)(((int32_t)dz_fp * t_ground) >> FX8_SHIFT);
    }
    
    /*=== SPHERE SHADING ===*/
    if (hit_sphere && (!hit_ground || t_hit < t_ground)) {
        int16_t hx = (int16_t)(((int32_t)dx_fp * t_hit) >> FX8_SHIFT);
        int16_t hy = (CAM_Y << FX8_SHIFT) + (int16_t)(((int32_t)dy_fp * t_hit) >> FX8_SHIFT);
        int16_t hz = (int16_t)(((int32_t)dz_fp * t_hit) >> FX8_SHIFT);
        
        /* Normal = hit point - sphere center */
        int16_t nx = hx;
        int16_t ny = hy - (SPHERE_CY << FX8_SHIFT);
        int16_t nz = hz - (SPHERE_CZ << FX8_SHIFT);
        nx >>= 1;
        ny >>= 1;
        nz >>= 1;
        
        /* Light direction (adjusted for view) */
        int16_t lx = light_dir_x * LIGHT_X;
        int16_t ly = LIGHT_Y;
        int16_t lz = light_dir_z * LIGHT_Z;
        
        /* Lambert shading */
        int32_t dot = ((int32_t)nx * lx + (int32_t)ny * ly + (int32_t)nz * lz) >> FX8_SHIFT;
        
        int16_t brightness = 50;  /* Ambient */
        if (dot > 0) {
            brightness += (dot * 205) >> 8;
        }
        if (brightness > 255) brightness = 255;
        
        return dither((uint8_t)brightness, COLOR_SHADOW, COLOR_SPHERE, px, py);
    }
    
    /*=== GROUND SHADING WITH DIRECTIONAL SHADOW (FULLY OPTIMIZED) ===*/
    if (hit_ground) {
        /* OPTIMIZATION 5: Use precomputed shadow center (no division!) */
        /* OPTIMIZATION 6: Use precomputed shadow_dz_sq from scanline */
        int32_t shadow_dx = (int32_t)ground_x - shadow_center_x_const;
        int32_t shadow_dx_sq = (shadow_dx * shadow_dx) >> FX8_SHIFT;
        int32_t shadow_dist_sq = shadow_dx_sq + scanline_shadow_dz_sq[py];
        
        /* OPTIMIZATION 4: Use shadow LUT (no division!) */
        /* Clamp to valid range BEFORE casting to prevent overflow */
        int16_t max_lut_value = (SHADOW_LUT_SIZE - 1) << SHADOW_LUT_SHIFT;
        uint8_t lut_idx;
        
        if (shadow_dist_sq >= max_lut_value) {
            lut_idx = SHADOW_LUT_SIZE - 1;  /* Far from shadow = full brightness */
        } else if (shadow_dist_sq <= 0) {
            lut_idx = 0;
        } else {
            lut_idx = (uint8_t)(shadow_dist_sq >> SHADOW_LUT_SHIFT);
        }
        
        uint8_t brightness = shadow_brightness_lut[lut_idx];
        
        return dither(brightness, COLOR_SHADOW, COLOR_GROUND, px, py);
    }
    
    return COLOR_SKY;
}

/*============================================================================
 * TILE GENERATION & STORAGE
 *============================================================================*/

void raytracer_render_row(uint8_t tile_row) {
    uint8_t base_py = tile_row * 8;
    
    for (uint8_t tx = 0; tx < RENDER_TILES_X; tx++) {
        uint8_t *tile_data = &tile_row_buffer[tx * 16];
        uint8_t base_px = tx * 8;
        
        for (uint8_t row = 0; row < 8; row++) {
            uint8_t low_bits = 0;
            uint8_t high_bits = 0;
            uint8_t py = base_py + row;
            
            for (uint8_t col = 0; col < 8; col++) {
                uint8_t color = trace_ray(base_px + col, py);
                uint8_t bit_pos = 7 - col;
                
                if (color & 0x01) low_bits  |= (1 << bit_pos);
                if (color & 0x02) high_bits |= (1 << bit_pos);
            }
            
            tile_data[row * 2]     = low_bits;
            tile_data[row * 2 + 1] = high_bits;
        }
    }
}

void raytracer_upload_row(uint8_t tile_row) {
    uint8_t tile_start = RENDER_TILE_BASE + tile_row * RENDER_TILES_X;
    set_bkg_data(tile_start, RENDER_TILES_X, tile_row_buffer);
}

void raytracer_store_row(uint8_t view_id, uint8_t tile_row) {
    memcpy(&scene_buffer[view_id][tile_row * RENDER_TILES_X * 16], 
           tile_row_buffer, RENDER_TILES_X * 16);
}

void raytracer_store_scene(uint8_t view_id) {
    (void)view_id;
}

void raytracer_load_scene(uint8_t view_id) {
    set_bkg_data(RENDER_TILE_BASE, MAX_RENDER_TILES, scene_buffer[view_id]);
}

/*============================================================================
 * PUBLIC API: INITIALIZATION
 *============================================================================*/

void raytracer_init(void) {
    /* Initialize all LUTs and precomputed arrays once at startup */
    init_luts();
    init_shadow_lut();
    init_dx_dy_arrays();
    init_ground_scanlines();
}

void raytracer_init_vram(void) {
    uint8_t map_offset_x = RENDER_OFFSET_X / 8;
    uint8_t map_offset_y = RENDER_OFFSET_Y / 8;
    
    /* Border tile (color 3 = sky) */
    uint8_t border_tile[16];
    memset(border_tile, 0xFF, 16);
    set_bkg_data(0, 1, border_tile);
    
    /* Clear render tiles */
    uint8_t empty_tile[16];
    memset(empty_tile, 0x00, 16);
    for (uint8_t i = 0; i < MAX_RENDER_TILES; i++) {
        set_bkg_data(RENDER_TILE_BASE + i, 1, empty_tile);
    }
    
    /* Fill tile map with border */
    VBK_REG = VBK_TILES;
    uint8_t border_row[32];
    memset(border_row, 0, 32);
    for (uint8_t y = 0; y < 18; y++) {
        set_bkg_tiles(0, y, 32, 1, border_row);
    }
    
    /* Set render area tile indices */
    for (uint8_t ty = 0; ty < RENDER_TILES_Y; ty++) {
        uint8_t tile_row_map[RENDER_TILES_X];
        for (uint8_t tx = 0; tx < RENDER_TILES_X; tx++) {
            tile_row_map[tx] = RENDER_TILE_BASE + ty * RENDER_TILES_X + tx;
        }
        set_bkg_tiles(map_offset_x, map_offset_y + ty, RENDER_TILES_X, 1, tile_row_map);
    }
}

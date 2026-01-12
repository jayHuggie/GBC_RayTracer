/**
 * raytracer.c - Colorful fixed-point raytracer with dithering
 * 
 * SMOOTH SHADING VIA DITHERING:
 * Since GBC only has 4 colors per palette, we use ordered dithering
 * (Bayer matrix pattern) to create the illusion of intermediate shades
 * by mixing two colors in a checkerboard-like pattern.
 * 
 * This gives us smooth gradients on the sphere and soft shadow edges!
 */

#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "raytracer.h"

/*============================================================================
 * TILE DATA BUFFER
 *============================================================================*/

static uint8_t tile_row_buffer[RENDER_TILES_X * 16];

/*============================================================================
 * DITHERING
 * 
 * 2x2 Bayer matrix for ordered dithering:
 *   | 0  2 |    Thresholds: 0, 128, 64, 192
 *   | 3  1 |    
 * 
 * For a given brightness (0-255), compare against the threshold
 * at position (x%2, y%2) to decide which color to use.
 *============================================================================*/

/* Bayer 2x2 thresholds (scaled to 0-255 range) */
static const uint8_t bayer2x2[2][2] = {
    {  0, 128 },
    { 192,  64 }
};

/**
 * Apply dithering to choose between two colors based on brightness.
 * 
 * @param brightness  Shade value 0-255 (0=dark, 255=bright)
 * @param dark_color  Color index to use for dark
 * @param bright_color Color index to use for bright
 * @param px, py      Pixel coordinates (for dither pattern)
 * @return Chosen color index
 */
static uint8_t dither(uint8_t brightness, uint8_t dark_color, uint8_t bright_color, 
                      uint8_t px, uint8_t py) {
    uint8_t threshold = bayer2x2[py & 1][px & 1];
    return (brightness > threshold) ? bright_color : dark_color;
}

/*============================================================================
 * RAY TRACING WITH SMOOTH DITHERED SHADING
 *============================================================================*/

static uint8_t trace_ray(uint8_t px, uint8_t py) {
    /* Ray direction calculation */
    int16_t half_w = RENDER_WIDTH / 2;
    int16_t half_h = RENDER_HEIGHT / 2;
    
    int16_t dx_fp = ((int16_t)px - half_w) * 5;
    int16_t dy_fp = (half_h - (int16_t)py) * 5;
    int16_t dz_fp = FX8_ONE;
    
    /* Vector from camera to sphere center */
    int16_t oc_y_fp = (SPHERE_CY - CAM_Y) << FX8_SHIFT;
    int16_t oc_z_fp = SPHERE_CZ << FX8_SHIFT;
    
    /* Sphere intersection math */
    int32_t oc_dot_d = ((int32_t)oc_y_fp * dy_fp + (int32_t)oc_z_fp * dz_fp) >> FX8_SHIFT;
    int32_t d_dot_d = ((int32_t)dx_fp * dx_fp + (int32_t)dy_fp * dy_fp + 
                       (int32_t)dz_fp * dz_fp) >> FX8_SHIFT;
    
    if (d_dot_d == 0) d_dot_d = 1;
    
    int32_t oc_sq = SPHERE_CZ * SPHERE_CZ;
    int32_t proj_sq = (oc_dot_d * oc_dot_d) / d_dot_d;
    int32_t dist_sq_fp = (oc_sq << FX8_SHIFT) - proj_sq;
    int32_t radius_sq_fp = SPHERE_R_SQ << FX8_SHIFT;
    
    uint8_t hit_sphere = 0;
    int16_t t_hit = 0;
    
    if (dist_sq_fp < radius_sq_fp && oc_dot_d > 0) {
        hit_sphere = 1;
        t_hit = (int16_t)(((int32_t)oc_dot_d << FX8_SHIFT) / d_dot_d);
    }
    
    /* Ground intersection */
    uint8_t hit_ground = 0;
    int16_t t_ground = 32767;
    int16_t ground_x = 0, ground_z = 0;
    
    if (dy_fp < -16) {
        t_ground = (int16_t)(((int32_t)(-CAM_Y) << (FX8_SHIFT + FX8_SHIFT)) / dy_fp);
        
        if (t_ground > 16 && t_ground < 2000) {
            hit_ground = 1;
            ground_x = (int16_t)(((int32_t)dx_fp * t_ground) >> FX8_SHIFT);
            ground_z = (int16_t)(((int32_t)dz_fp * t_ground) >> FX8_SHIFT);
        }
    }
    
    /*==========================================================================
     * SPHERE SHADING WITH DITHERING
     *==========================================================================*/
    if (hit_sphere && (!hit_ground || t_hit < t_ground)) {
        /* Compute hit point and normal */
        int16_t hx = (int16_t)(((int32_t)dx_fp * t_hit) >> FX8_SHIFT);
        int16_t hy = (CAM_Y << FX8_SHIFT) + (int16_t)(((int32_t)dy_fp * t_hit) >> FX8_SHIFT);
        int16_t hz = (int16_t)(((int32_t)dz_fp * t_hit) >> FX8_SHIFT);
        
        int16_t nx = hx;
        int16_t ny = hy - (SPHERE_CY << FX8_SHIFT);
        int16_t nz = hz - (SPHERE_CZ << FX8_SHIFT);
        nx >>= 1;
        ny >>= 1;
        nz >>= 1;
        
        /* Lambert shading */
        int32_t dot = ((int32_t)nx * LIGHT_X + (int32_t)ny * LIGHT_Y + 
                       (int32_t)nz * LIGHT_Z) >> FX8_SHIFT;
        
        /* Convert dot product to brightness (0-255) */
        /* Add ambient light (~20%) so even dark side isn't pure black */
        int16_t brightness = 50;  /* Ambient base */
        if (dot > 0) {
            brightness += (dot * 205) >> 8;  /* Scale diffuse to fill 0-255 */
        }
        if (brightness > 255) brightness = 255;
        
        /* Dither between shadow (dark) and sphere color (red) */
        return dither((uint8_t)brightness, COLOR_SHADOW, COLOR_SPHERE, px, py);
    }
    
    /*==========================================================================
     * GROUND SHADING WITH DIRECTIONAL SHADOW
     * 
     * Project sphere center onto ground along light direction.
     * Light comes from upper-left (-X, +Y), so shadow falls to the right (+X).
     * 
     * Shadow center = SpherePos + t * (-LightDir), where t brings us to y=0
     * t = SphereY / LightY (how far to travel in -LightDir to reach ground)
     *==========================================================================*/
    if (hit_ground) {
        /* Calculate shadow center position (projected along light direction) */
        /* t = sphere_height / light_y_component */
        /* In 8.8: t = (SPHERE_CY << 16) / LIGHT_Y */
        int32_t t_shadow = ((int32_t)SPHERE_CY << 16) / LIGHT_Y;  /* 8.8 result */
        
        /* Shadow center offset from sphere XZ position */
        /* shadow_x = sphere_x + t * (-light_x) = 0 + t * 0.5 (light_x is -0.5) */
        /* shadow_z = sphere_z + t * (-light_z) = 6 - t * 0.5 */
        int32_t shadow_center_x = (SPHERE_CX << FX8_SHIFT) + 
                                  (((-LIGHT_X) * t_shadow) >> FX8_SHIFT);
        int32_t shadow_center_z = (SPHERE_CZ << FX8_SHIFT) + 
                                  (((-LIGHT_Z) * t_shadow) >> FX8_SHIFT);
        
        /* Distance from ground hit point to shadow center */
        int32_t shadow_dx = ground_x - shadow_center_x;
        int32_t shadow_dz = ground_z - shadow_center_z;
        int32_t shadow_dist_sq = (shadow_dx * shadow_dx + shadow_dz * shadow_dz) >> FX8_SHIFT;
        
        /* Shadow has two zones:
         * - Inner 50% (umbra): solid dark
         * - Outer 50% (penumbra): fades from dark to lit
         */
        int32_t shadow_radius_sq = (int32_t)SPHERE_R_SQ << FX8_SHIFT;
        int32_t umbra_radius_sq = shadow_radius_sq >> 2;  /* 25% of area = 50% of radius */
        
        uint8_t brightness;
        
        if (shadow_dist_sq >= shadow_radius_sq) {
            /* Outside shadow - fully lit */
            brightness = 255;
        } else if (shadow_dist_sq <= umbra_radius_sq) {
            /* Inside umbra (core) - solid dark */
            brightness = 0;
        } else {
            /* Penumbra - fade from dark to lit */
            /* Map distance from umbra edge to shadow edge -> 0 to 255 */
            int32_t penumbra_range = shadow_radius_sq - umbra_radius_sq;
            int32_t dist_in_penumbra = shadow_dist_sq - umbra_radius_sq;
            int32_t ratio = (dist_in_penumbra * 256) / penumbra_range;
            brightness = (uint8_t)ratio;
        }
        
        /* Dither between shadow and ground color */
        return dither(brightness, COLOR_SHADOW, COLOR_GROUND, px, py);
    }
    
    /*==========================================================================
     * SKY
     *==========================================================================*/
    return COLOR_SKY;
}

/*============================================================================
 * TILE GENERATION
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

void raytracer_init_vram(void) {
    uint8_t map_offset_x = RENDER_OFFSET_X / 8;
    uint8_t map_offset_y = RENDER_OFFSET_Y / 8;
    
    /* Create border tile */
    uint8_t border_tile[16];
    memset(border_tile, 0xFF, 16);
    set_bkg_data(0, 1, border_tile);
    
    /* Clear render tiles */
    uint8_t empty_tile[16];
    memset(empty_tile, 0x00, 16);
    for (uint8_t i = 0; i < MAX_RENDER_TILES; i++) {
        set_bkg_data(RENDER_TILE_BASE + i, 1, empty_tile);
    }
    
    /* Set up tile map */
    VBK_REG = VBK_TILES;
    
    uint8_t border_row[32];
    memset(border_row, 0, 32);
    for (uint8_t y = 0; y < 18; y++) {
        set_bkg_tiles(0, y, 32, 1, border_row);
    }
    
    for (uint8_t ty = 0; ty < RENDER_TILES_Y; ty++) {
        uint8_t tile_row_map[RENDER_TILES_X];
        for (uint8_t tx = 0; tx < RENDER_TILES_X; tx++) {
            tile_row_map[tx] = RENDER_TILE_BASE + ty * RENDER_TILES_X + tx;
        }
        set_bkg_tiles(map_offset_x, map_offset_y + ty, RENDER_TILES_X, 1, tile_row_map);
    }
}

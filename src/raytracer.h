/**
 * raytracer.h - Fixed-point raytracer for Game Boy Color
 * 
 * FIXED-POINT FORMAT:
 * We use 8.8 fixed-point (16-bit signed integer with 8 fractional bits).
 * Range: approximately -128.0 to +127.996
 */

#ifndef _RAYTRACER_H
#define _RAYTRACER_H

#include <stdint.h>

/*============================================================================
 * FIXED-POINT MATH (8.8 format)
 *============================================================================*/

typedef int16_t fixed8_t;

#define FX8_SHIFT       8
#define FX8_ONE         (1 << FX8_SHIFT)        /* 1.0 = 256 */
#define FX8_HALF        (FX8_ONE >> 1)          /* 0.5 = 128 */

#define INT_TO_FX8(x)   ((fixed8_t)((x) << FX8_SHIFT))
#define FX8_TO_INT(x)   ((int8_t)((x) >> FX8_SHIFT))
#define FX8_MUL(a, b)   ((fixed8_t)(((int32_t)(a) * (int32_t)(b)) >> FX8_SHIFT))

/*============================================================================
 * SCENE CONFIGURATION
 *============================================================================*/

/* Render window: 96x96 pixels (12x12 tiles = 144 tiles) */
#define RENDER_WIDTH    96
#define RENDER_HEIGHT   96

#define RENDER_TILES_X  (RENDER_WIDTH / 8)   /* 12 */
#define RENDER_TILES_Y  (RENDER_HEIGHT / 8)  /* 12 */

#define RENDER_OFFSET_X ((160 - RENDER_WIDTH) / 2)   /* 32 */
#define RENDER_OFFSET_Y ((144 - RENDER_HEIGHT) / 2)  /* 24 */

#define RENDER_TILE_BASE 1
#define MAX_RENDER_TILES (RENDER_TILES_X * RENDER_TILES_Y)

/*============================================================================
 * SCENE OBJECTS
 *============================================================================*/

/* Sphere at (0, 2, 6), radius 2 */
#define SPHERE_CX       0
#define SPHERE_CY       2
#define SPHERE_CZ       6
#define SPHERE_R        2
#define SPHERE_R_SQ     4

/* Camera at (0, 2, 0) */
#define CAM_Y           2

/* Light direction (8.8 fixed): (-0.5, 0.7, 0.5) */
#define LIGHT_X         (-128)
#define LIGHT_Y         (179)
#define LIGHT_Z         (128)

/*============================================================================
 * COLOR-MAPPED SHADE VALUES
 * 
 * These map to palette colors:
 *   0 = Shadow (dark purple)
 *   1 = Sphere color (red)
 *   2 = Ground color (green)
 *   3 = Sky color (blue)
 *============================================================================*/

#define COLOR_SHADOW    0
#define COLOR_SPHERE    1
#define COLOR_GROUND    2
#define COLOR_SKY       3

/*============================================================================
 * FUNCTION DECLARATIONS
 *============================================================================*/

void raytracer_render_row(uint8_t tile_row);
void raytracer_init_vram(void);
void raytracer_upload_row(uint8_t tile_row);

#endif /* _RAYTRACER_H */

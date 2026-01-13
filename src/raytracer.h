/**
 * raytracer.h - Fixed-point raytracer with 4-view gallery
 */

#ifndef _RAYTRACER_H
#define _RAYTRACER_H

#include <stdint.h>

/*============================================================================
 * FIXED-POINT MATH (8.8 format)
 *============================================================================*/

typedef int16_t fixed8_t;

#define FX8_SHIFT       8
#define FX8_ONE         (1 << FX8_SHIFT)
#define FX8_HALF        (FX8_ONE >> 1)

#define INT_TO_FX8(x)   ((fixed8_t)((x) << FX8_SHIFT))
#define FX8_TO_INT(x)   ((int8_t)((x) >> FX8_SHIFT))
#define FX8_MUL(a, b)   ((fixed8_t)(((int32_t)(a) * (int32_t)(b)) >> FX8_SHIFT))

/*============================================================================
 * SCENE CONFIGURATION
 *============================================================================*/

#define RENDER_WIDTH    96
#define RENDER_HEIGHT   96

#define RENDER_TILES_X  (RENDER_WIDTH / 8)   /* 12 */
#define RENDER_TILES_Y  (RENDER_HEIGHT / 8)  /* 12 */

#define RENDER_OFFSET_X ((160 - RENDER_WIDTH) / 2)
#define RENDER_OFFSET_Y ((144 - RENDER_HEIGHT) / 2)

#define RENDER_TILE_BASE 1
#define MAX_RENDER_TILES (RENDER_TILES_X * RENDER_TILES_Y)  /* 144 */

/* Bytes per scene (144 tiles * 16 bytes) */
#define SCENE_SIZE      (MAX_RENDER_TILES * 16)  /* 2304 bytes */

/*============================================================================
 * CAMERA VIEWS
 *============================================================================*/

#define VIEW_FRONT      0   /* Down button - original view */
#define VIEW_BACK       1   /* Up button - behind sphere */
#define NUM_VIEWS       2

/*============================================================================
 * SCENE OBJECTS
 *============================================================================*/

#define SPHERE_CX       0
#define SPHERE_CY       2
#define SPHERE_CZ       6   /* Sphere 6 units in front of camera */
#define SPHERE_R        2
#define SPHERE_R_SQ     4

#define CAM_Y           2   /* Camera height */

#define LIGHT_X         (-128)
#define LIGHT_Y         (179)
#define LIGHT_Z         (128)

/*============================================================================
 * COLORS
 *============================================================================*/

#define COLOR_SHADOW    0
#define COLOR_SPHERE    1
#define COLOR_GROUND    2
#define COLOR_SKY       3

/*============================================================================
 * FUNCTION DECLARATIONS
 *============================================================================*/

/* Initialize LUTs and precomputed arrays (call once at startup) */
void raytracer_init(void);

/* Set camera angle for rendering (0=front, 64=right, 128=back, 192=left) */
void raytracer_set_view(uint8_t view_id);

/* Render one row of the current view */
void raytracer_render_row(uint8_t tile_row);

/* Upload rendered row to VRAM */
void raytracer_upload_row(uint8_t tile_row);

/* Initialize VRAM structures */
void raytracer_init_vram(void);

/* Store rendered row to scene buffer */
void raytracer_store_row(uint8_t view_id, uint8_t tile_row);

/* Store current rendered scene to buffer (not used with per-row storage) */
void raytracer_store_scene(uint8_t view_id);

/* Load pre-rendered scene from buffer to VRAM */
void raytracer_load_scene(uint8_t view_id);

/* Trig functions */
fixed8_t fx_sin(uint8_t angle);
fixed8_t fx_cos(uint8_t angle);

#endif

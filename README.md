# GBC RayTracer
![Language](https://img.shields.io/badge/Language-C-00599C?style=for-the-badge)
![Platform](https://img.shields.io/badge/Platform-Game%20Boy%20Color-00A9A5?style=for-the-badge)
![Toolchain](https://img.shields.io/badge/Toolchain-GBDK--2020-FF6F00?style=for-the-badge)

A Game Boy Color homebrew ROM that renders a raytraced 3D scene with real-time shading and shadows.

### *Actual Footage:*

<img src="/scenes/live_demo.gif" alt="Live Demo" title="Live Demo" width="400"/>

### *Emulator Footage:*

<img src="/scenes/emulation_demo.gif" alt="Emulation Demo" title="Emulation Demo" width="400"/>



## Features

- **Title Screen** with colorful animated-style "CoLoR" text
  
  <img src="/scenes/title_screen.png" alt="Title Screen" title="Title Screen" width="200"/>
  
- **96×96 pixel** render window (12×12 tiles)
- **Lambert shading** with smooth dithered gradients on the sphere
- **Soft shadows** with umbra (dark core) and penumbra (soft edge)
- **2-view gallery** - switch between close and far views with D-pad
- **Progress bar** shows rendering progress in real-time
- **Runs on real hardware** - optimized for GBC's limited CPU

## Screenshots

<img src="/scenes/scene_front.png" alt="Front Scene" title="Front Scene" width="250"/> <img src="/scenes/scene_back.png" alt="Back Scene" title="Back Scene" width="250"/>

The ROM displays:
- A red shaded sphere with smooth Lambert lighting
- A green ground plane
- A realistic soft shadow with dark center and gradual falloff
- Sky blue background
- Dark border frame around the render window

## Controls

| Button | Action |
|--------|--------|
| **START / A** | Begin rendering (from title screen) |
| **D-pad Down** | Switch to close view (large sphere) |
| **D-pad Up** | Switch to far view (small sphere) |

## Building

### Prerequisites

1. **GBDK-2020** - The Game Boy Development Kit
   - Download from: https://github.com/gbdk-2020/gbdk-2020/releases
   - Install to `~/gbdk/` (default) or set `GBDK_HOME` environment variable

2. **Make** - Standard build tool (pre-installed on macOS/Linux)

### Build Commands

```bash
# Build the ROM (outputs GBC_RayTracer.gbc)
make

# Clean build artifacts
make clean
```

### Output

The build produces `GBC_RayTracer.gbc`, a valid Game Boy Color ROM file (~32KB).

## Running

### Recommended Emulators

- [Emulicious](https://emulicious.net) - Multi-system emulator for Mac OS Windows, Linux, Raspberry Pi OS,
- [BGB Emulator](https://bgb.bircd.org) - Hardware accurate *Windows Native* emulator

### Real Hardware

Flash the `.gbc` file to a flash cart (e.g., EverDrive GB, EZ-Flash Jr.) and run on:
- Game Boy Color
- Game Boy Advance / SP

## Technical Details

### Fixed-Point Math

All calculations use 8.8 fixed-point arithmetic (16-bit signed integers with 8 fractional bits):
- Range: -128.0 to +127.996
- Precision: 1/256 ≈ 0.004

No floating point is used anywhere in the codebase.

### Optimizations

The raytracer uses several optimizations to achieve reasonable render times on GBC:

1. **Lookup Tables (LUTs)** - Sphere intersection divisions replaced with 64-entry LUTs
2. **Per-scanline precomputation** - Ground intersection calculated once per row
3. **Precomputed dx/dy arrays** - Ray directions cached for all screen coordinates
4. **Shadow brightness LUT** - Penumbra falloff via 128-entry table (no division)
5. **Per-view shadow constants** - Shadow center precomputed once per scene
6. **Scanline-by-scanline rendering** - Smooth visual feedback during render

### Ray Tracing Algorithm

For each pixel in the 96×96 render window:

1. **Generate ray** from camera through pixel center
2. **Intersect sphere** using optimized LUT-based quadratic solver
3. **Intersect ground plane** using precomputed scanline values
4. **Shade hit point**:
   - Sphere: Lambert diffuse with 2×2 Bayer dithering
   - Ground: Solid color with soft shadow (umbra + penumbra)
5. **Dither** brightness to 4-color palette

### Shadow Technique

**Soft Blob Shadow** with realistic falloff:
- **Umbra**: Dark core directly under sphere (25% of shadow radius)
- **Penumbra**: Gradual falloff from umbra edge to full brightness
- Shadow position calculated from light direction projection

### Tile-Based Rendering

The GBC uses 8×8 pixel tiles. This raytracer:
1. Renders scanline-by-scanline with progress bar
2. Uploads each scanline to VRAM during VBlank
3. Stores completed scenes in RAM (2 views × 2304 bytes)
4. Allows instant switching between pre-rendered views

### Memory Layout

- **Scene Buffer**: 4,608 bytes (2 views × 144 tiles × 16 bytes/tile)
- **Tile Row Buffer**: 192 bytes (12 tiles × 16 bytes)
- **LUTs**: ~500 bytes (sphere, shadow, dx/dy arrays)
- **Title Screen**: ~2KB (tiles + map)
- **Total ROM**: ~32KB

## File Structure

```
GBC_RayTracer/
├── Makefile              # Build configuration
├── README.md             # This file
├── TitleScreen.png       # Title screen source image
├── res/                  # Resources folder
└── src/
    ├── main.c            # Entry point, title screen, game loop
    ├── raytracer.c       # Ray tracing, LUTs, scene rendering
    ├── raytracer.h       # Fixed-point math, scene config
    ├── graphics.c        # Palette setup, color definitions
    ├── graphics.h        # Graphics function declarations
    ├── util.h            # LCD control macros
    ├── TitleScreen.c     # Generated title screen tile data
    └── TitleScreen.h     # Title screen declarations
```

## Scene Configuration

Edit `raytracer.h` to modify the scene:

```c
/* Render window size */
#define RENDER_WIDTH    96
#define RENDER_HEIGHT   96

/* Sphere position and size */
#define SPHERE_CX       0   /* Center X */
#define SPHERE_CY       2   /* Height above ground */
#define SPHERE_CZ       6   /* Distance from camera */
#define SPHERE_R        2   /* Radius */

/* Light direction */
#define LIGHT_X         (-128)  /* From left */
#define LIGHT_Y         (179)   /* From above */
#define LIGHT_Z         (128)   /* From front */

/* Colors (palette indices) */
#define COLOR_SHADOW    0   /* Dark purple */
#define COLOR_SPHERE    1   /* Red */
#define COLOR_GROUND    2   /* Green */
#define COLOR_SKY       3   /* Sky blue */
```

## Creating a Custom Title Screen

1. Create a 160×144 PNG image (GBC screen size)
2. Use indexed color mode with max 4 colors per 8×8 tile region
3. Convert with png2asset:
   ```bash
   ~/gbdk/bin/png2asset TitleScreen.png -c TitleScreen.c -map -noflip
   ```
4. Move generated files to `src/` folder
5. Adjust palette attributes in `main.c` if using multiple palettes

## License

MIT License - See LICENSE file

## Author

**Jae Hyuk Lee**

## Credits

- Built with [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020)
- Title screen created with [LibreSprite](https://libresprite.github.io/)

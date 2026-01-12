# GBC RayTracer

A minimal Game Boy Color homebrew ROM that renders a raytraced scene.

![Scene](https://img.shields.io/badge/Scene-Sphere%20%2B%20Ground-blue)
![Platform](https://img.shields.io/badge/Platform-Game%20Boy%20Color-green)

## Features

- **One sphere** above a ground plane
- **Lambert shading** on the sphere (light · normal diffuse lighting)
- **Blob shadow** on the ground (projected circle shadow under sphere)
- **4 brightness levels** using GBC palette
- **48×48 pixel** render window (6×6 tiles)
- **Runs on real hardware** at low FPS (rendering done once at boot)

## Scene Preview

The ROM displays:
- A shaded sphere with visible light falloff from Lambert shading
- A checkered ground plane for visual interest
- A circular shadow directly under the sphere
- A dark blue border around the render window

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

# Build with debug output
make GBDK_DEBUG=ON

# Use custom GBDK path
make GBDK_HOME=/path/to/gbdk/
```

### Output

The build produces `GBC_RayTracer.gbc`, a valid Game Boy Color ROM file.

## Running

### Emulators (Recommended for Development)

- **BGB** (Windows) - Excellent debugging features
- **SameBoy** (macOS/Linux/Windows) - Accurate GBC emulation
- **mGBA** - Cross-platform, accurate

### Real Hardware

Flash the `.gbc` file to a flash cart (e.g., EverDrive GB, EZ-Flash Jr.) and run on:
- Game Boy Color
- Game Boy Advance / SP (backwards compatible)

## Technical Details

### Fixed-Point Math

All calculations use 8.8 fixed-point arithmetic (16-bit signed integers with 8 fractional bits):
- Range: -128.0 to +127.996
- Precision: 1/256 ≈ 0.004

No floating point is used anywhere in the codebase.

### Ray Tracing Algorithm

For each pixel in the 48×48 render window:

1. **Generate ray** from camera through pixel center
2. **Intersect sphere** using quadratic formula
3. **Intersect ground plane** at Y=0
4. **Shade hit point**:
   - Sphere: Lambert diffuse = max(0, normal · light_dir)
   - Ground: Checkerboard pattern + blob shadow check

### Shadow Technique

**Blob Shadow (Option B)** was chosen over true shadow rays because:
1. **Simpler**: Just check if ground point is within projected radius
2. **Faster**: No secondary ray-sphere intersection needed
3. **Adequate**: Creates a visually clear circular shadow

The shadow is computed by projecting the sphere center onto the ground and darkening pixels within the sphere's radius of that point.

### Tile-Based Rendering

The GBC uses 8×8 pixel tiles. This raytracer:
1. Pre-renders all 36 tiles (6×6 grid) at startup
2. Uploads tile data to VRAM
3. Sets up the tile map to display the render window
4. Scene is static, so no per-frame updates needed

### Memory Layout

- **Tile Data**: 576 bytes (36 tiles × 16 bytes/tile)
- **Tile Map**: 36 bytes
- **Code + Data**: ~3-4KB total ROM

## File Structure

```
GBC_RayTracer/
├── Makefile              # Build configuration
├── README.md             # This file
└── src/
    ├── main.c            # Entry point, initialization
    ├── raytracer.c       # Ray tracing and tile generation
    ├── raytracer.h       # Fixed-point math, scene config
    ├── graphics.c        # Palette setup
    ├── graphics.h        # Color definitions
    └── util.h            # LCD control macros
```

## Scene Configuration

Edit `raytracer.h` to modify the scene:

```c
/* Sphere position and size */
#define SPHERE_X        INT_TO_FX8(0)   /* Center X */
#define SPHERE_Y        INT_TO_FX8(2)   /* Height above ground */
#define SPHERE_Z        INT_TO_FX8(6)   /* Distance from camera */
#define SPHERE_RADIUS   INT_TO_FX8(2)   /* Size */

/* Light direction (normalized) */
#define LIGHT_DIR_X     (-128)          /* -0.5 */
#define LIGHT_DIR_Y     (179)           /* ~0.7 */
#define LIGHT_DIR_Z     (128)           /* 0.5 */
```

## License

MIT License - See LICENSE file

## Credits

Built with [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020)

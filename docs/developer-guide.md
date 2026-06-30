# Map Renderer v2 — Developer Guide

## Prerequisites

- **OS:** Linux (primary development platform)
- **Compiler:** g++ with C++23 support (gcc 13+)
- **Build system:** CMake 3.20+
- **Dependencies:**
  - Vulkan SDK (vulkan-headers, vulkan-icd-loader, glslang)
  - Protocol Buffers (protobuf)
  - GLM (OpenGL Mathematics, header-only)
  - SDL2 (Simple DirectMedia Layer, for windowing)

## Installation

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config \
    libvulkan-dev vulkan-tools glslang-tools \
    libprotobuf-dev protobuf-compiler \
    libglm-dev \
    libsdl2-dev
```

### Arch Linux

```bash
sudo pacman -S cmake pkg-config \
    vulkan-devel vulkan-icd-loader glslang \
    protobuf glm \
    sdl2
```

## Building

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

Two executables are produced:
- `map-renderer` — main application
- `test_mvt` — standalone MVT parser test

## Running

### Main application

```bash
# From build directory
./map-renderer

# The app loads data/style.json and data/test_roads.mvt by default
```

The window opens at 1024x768. Use arrow keys to pan, +/- to zoom, mouse wheel to zoom, left mouse drag to pan, ESC to quit.

### MVT parser test

```bash
./test_mvt data/test_roads.mvt
```

Prints a summary of layers, feature counts, and a sample geometry command.

### Other test tiles

```bash
./test_mvt data/test_minimal.mvt
./test_mvt data/test_real.mvt
./test_mvt data/test_polygons.mvt
./test_mvt data/z14.mvt
./test_mvt test_data/chicago.mvt
./test_mvt test_data/sf.mvt
```

## Project Structure

```
.
├── CMakeLists.txt              # Build configuration
├── README.md                   # Quick start
├── SPEC.md                     # Technical specification
├── docs/                       # This documentation
│   ├── api-reference.md
│   ├── architecture.md
│   └── developer-guide.md
├── src/
│   ├── main.cpp                # Full Vulkan application
│   ├── mvt_parser.h            # MVT PBF parser (header-only)
│   ├── render_data.h           # Geometry conversion (header-only)
│   ├── style_engine.h          # JSON style parser (header-only)
│   ├── test_mvt.cpp            # MVT parser test program
│   └── shaders/                # GLSL sources
│       ├── triangle.vert/.frag
│       ├── line.vert/.frag
│       └── fill.vert/.frag
├── data/
│   ├── style.json              # Default style
│   ├── zurich.mvt              # Zurich tile
│   ├── z14.mvt                 # Zoom 14 tile
│   ├── test_minimal.mvt        # Minimal test tile
│   ├── test_real.mvt           # Real-world test tile
│   ├── test_roads.mvt          # Roads test tile
│   ├── test_sf.mvt             # San Francisco test tile
│   └── test_polygons.mvt       # Polygon test tile
├── test_data/
│   ├── chicago.mvt
│   └── sf.mvt
└── build/                      # Build output
    ├── map-renderer
    ├── test_mvt
    ├── data/                   # Copy of data/ (post-build)
    └── src/shaders/*.spv       # Compiled SPIR-V
```

## Implementing New Features

### Adding a new layer type to the style engine

1. Edit `src/style_engine.h` — add new paint property fields to `PaintProperties`
2. Add parsing logic in `StyleEngine::parse_paint()` for the new property name
3. Add the property to `StyleRule` if it needs to be returned to the renderer
4. Update `matchRule()` if the matching logic changes

### Adding a new geometry type

1. Add the geometry type to `mvt::GeomType` enum in `mvt_parser.h`
2. Add decoding logic in `render_data.h` (decode_linestring_geometry / decode_polygon_rings)
3. Add a new pipeline in `main.cpp` if the rendering is different
4. Add matching logic in `style_engine.h`

### Adding new shaders

1. Create `.vert` and `.frag` files in `src/shaders/`
2. Add `add_shader()` calls in `CMakeLists.txt`
3. Follow the existing shader conventions:
   - Vertex shader: `layout(binding = 0) uniform CameraUBO { mat4 proj; } camera;`
   - Push constants for per-draw parameters (color, etc.)
   - Output `fragColor` at location 0
4. Compile: `glslangValidator -V shader.vert -o shader.vert.spv`

## Debugging Tips

### Shader compilation errors
```bash
glslangValidator -V src/shaders/line.vert
```

### Check if Vulkan is working
```bash
vulkaninfo
```

### Print parsed MVT data
```bash
./test_mvt data/test_roads.mvt
```

### Build in debug mode
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

## Testing

The project has a single test program `test_mvt`. There is no automated test framework.

```bash
# Test all available tiles
for f in data/*.mvt test_data/*.mvt; do
    echo "=== $f ==="
    ./test_mvt "$f"
done
```

## Build Targets

```bash
# Clean build
rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Rebuild only shaders (no C++ recompilation)
cmake --build . --target map-renderer

# Install (if configured)
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
make install
```

## Code Conventions

- **C++23** features are used (ranges, concepts, etc.) where beneficial
- **No external dependencies** beyond Vulkan, GLM, SDL2, protobuf
- **Header-only modules** for parser, geometry conversion, and style engine
- **Single main.cpp** for the Vulkan application (all rendering code in one file)
- **No virtual functions** — all components are free functions or simple classes
- **Manual JSON parsing** — no JSON library dependency
- **Manual protobuf parsing** — no generated code, walks wire format directly
- **Compile warnings:** `-Wall -Wextra -Wpedantic` are enabled
- **Release builds:** `-O2 -DNDEBUG`

## Milestone Checklist

| # | Status | Description |
|---|--------|-------------|
| 1 | ✅ | Project skeleton (CMake + main.cpp) |
| 2 | ✅ | MVT parser reads tiles, prints feature counts |
| 3 | ✅ | Vulkan triangle rendered on screen |
| 4 | ✅ | JSON style parsing and color application |
| 5 | ✅ | Render MVT lines from real tile |
| 6 | ✅ | Render MVT polygons with fills |
| 7 | ✅ | Interactive pan and zoom |

## Future Work (Not Yet Implemented)

- **Glyph atlas:** SDF text rendering for map labels
- **Multiple tile loading:** Load tiles for the current viewport
- **Tile caching:** Cache decoded tiles to avoid re-parsing
- **LOD selection:** Select appropriate zoom levels for tiles
- **Point rendering:** Render point features as sprites/quadlets
- **Anti-aliasing:** MSAA or post-process AA
- **Shader improvements:** Gradient fills, line patterns, dashed lines
- **Performance:** Instancing, draw call batching, async tile loading

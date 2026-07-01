# Interactive 3D Map Renderer

A from-scratch 3D map renderer built with **Vulkan** and **C++23** that
renders OpenStreetMap data in both 2D top-down and 3D perspective views
with extruded buildings.

## Features

- 2D orthographic mode with pan, zoom, and ground grid
- 3D perspective mode with per-vertex-lit extruded buildings
- Dual-mode camera (2D ortho / 3D spherical orbit) with smooth controls
- Full Vulkan pipeline: instance → device → swapchain → render pass → pipelines
- Protobuf-based preprocessed OSM data pipeline
- Ear-clipping triangulation for polygon features
- 41 unit + integration tests (Google Test)
- Debug-logging infrastructure, sanitizer support

## Dependencies

| Dependency      | Minimum Version | Notes                        |
|-----------------|-----------------|------------------------------|
| C++ compiler    | GCC 12 / Clang 15 | C++23 support required    |
| CMake           | 3.20            |                              |
| Vulkan SDK      | 1.2             | `vulkan-headers`, `glslc`    |
| SDL2            | 2.0             | Window + input               |
| GLM             | 0.9.9           | Math library (header-only)   |
| protobuf        | 3.21            | C++ runtime + `protoc`       |
| nlohmann/json   | 3.11            | Fetched automatically        |
| Google Test     | 1.14            | Fetched automatically        |
| Python          | 3.8+            | OSM preprocessing            |

Arch Linux one-liner:
```bash
sudo pacman -S cmake vulkan-devel sdl2 glm protobuf gtest python-protobuf python-osmium
```

## Build

```bash
mkdir -p _build && cd _build
cmake ..
make -j$(nproc)
```

Optionally enable sanitizers:
```bash
cmake .. -DMAP_RENDERER_USE_ASAN=ON -DMAP_RENDERER_USE_UBSAN=ON
```

## Compile Shaders

```bash
bash tools/compile_shaders.sh
```

This compiles GLSL sources in `src/shaders/` to SPIR-V binaries in
`_build/shaders/`.  Requires `glslc` (Vulkan SDK).

## Run

```bash
./map-renderer
```

## Run Tests

```bash
./map-renderer-tests
```

## Controls

| Key       | 2D Mode           | 3D Mode             |
|-----------|-------------------|---------------------|
| Arrows/WASD | Pan             | Pan                 |
| `+` / `-` | Zoom in / out     | Zoom in / out       |
| Mouse drag | Pan              | Orbit (rotate+tilt) |
| Scroll    | Zoom              | Zoom (distance)     |
| `Q` / `E` | —                 | Tilt up / down      |
| `A` / `D` | Pan left/right    | Rotate left/right   |
| `F1`      | Switch to 2D mode | Switch to 2D mode   |
| `F2`      | Switch to 3D mode | Switch to 3D mode   |
| `ESC`     | Quit              | Quit                |

## Data Pipeline

```
OSM PBF  →  Python preprocessor  →  protobuf  →  C++ loader  →  GPU buffers  →  Display
           (WGS84→ENU, heights)     (binary)      (protobuf)     (Vulkan)
```

The Python tool `tools/extract_geometry.py` reads an OSM PBF file, converts
coordinates to local ENU meters, applies building height fallback rules, and
serialises everything into a compact protobuf binary consumed by the C++
renderer.

## Project Structure

```
src/core/       Window, input state, camera, Vulkan context
src/data/       OSM types, loader, geometry builder, style engine
src/graphics/   Vulkan wrappers (buffer, descriptor, shader, pipeline)
src/proto/      Protobuf schema definition (osm_data.proto)
src/render/     Renderer class (pipelines, buffers, draw orchestration)
src/shaders/    GLSL vertex/fragment shaders (2d/, 3d/)
tests/          Google Test unit + integration tests
tools/          Python preprocessing scripts
data/           Sample data and style definitions
specs/          Design documents (HLD, LLD, requirements, task breakdown)
docs/           Per-task build logs and notes
```

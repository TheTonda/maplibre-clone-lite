# Interactive 3D Map Renderer

A from-scratch 3D map renderer built with **Vulkan** and **C++23** that
renders OpenStreetMap data in both 2D top-down and 3D perspective views
with extruded buildings.

## Features

- 2D orthographic mode with pan, zoom, and ground grid
- 3D perspective mode with per-vertex-lit extruded buildings
- Smooth camera controls (keyboard + mouse)
- Protobuf-based preprocessed OSM data pipeline
- Comprehensive unit and integration tests
- Debug-logging infrastructure

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
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Run

```bash
./map-renderer
```

## Run Tests

```bash
./map-renderer-tests
```

## Controls

| Key            | Action                              |
|----------------|-------------------------------------|
| Arrow keys     | Pan                                 |
| `+` / `-`      | Zoom in / out                       |
| Mouse drag     | Pan                                 |
| Mouse scroll   | Zoom                                |
| `Q` / `E`      | Tilt (3D mode)                      |
| `A` / `D`      | Rotate (3D mode)                    |
| `F1`           | Switch to 2D mode                   |
| `F2`           | Switch to 3D mode                   |
| `ESC`          | Quit                                |

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
src/core/       Window, input, camera, renderer, Vulkan context
src/data/       OSM loader, geometry builder, style engine
src/graphics/   Vulkan wrappers (buffer, descriptor, pipeline, shader)
src/proto/      Protobuf schema definition
src/shaders/    GLSL vertex/fragment shaders (2d/, 3d/)
tests/          Google Test unit + integration tests
tools/          Python preprocessing scripts
data/           Sample data and style definitions
specs/          Design documents (HLD, LLD, requirements, task breakdown)
docs/           Per-task build logs and notes
```

## License

MIT

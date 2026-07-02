# High-Level Design (HLD) Specification
## Interactive 2D Map Renderer

**Version:** 2.0  
**Date:** July 3, 2026  
**Status:** Specification — Redesigned for 2D tiled rendering on Android

---

## 1. Executive Summary

This document describes the architecture of a 2D tiled map renderer for OpenStreetMap data. The renderer loads preprocessed map tiles from storage, renders them using OpenGL ES 3.0, and targets budget Android devices (specifically the Xiaomi Pad 6). The engine is built as a platform-independent library with thin desktop (SDL2) and Android (NativeActivity + EGL) application wrappers.

### 1.1 Goals
- Render OSM data as a 2D top-down map using slippy-map tiles
- Target 60 FPS on Xiaomi Pad 6 with New Delhi data
- Handle India-scale datasets (1.6 GB PBF) via tiling and LRU cache
- Keep memory < 150 MB RAM, < 50 MB GPU
- Keep compiled tile data compact for limited device storage
- Same engine runs on Linux desktop and Android
- Architecture supports future 3D rendering without redesign

### 1.2 Non-Goals (v2.0)
- 3D rendering, building extrusion, lighting
- Vulkan
- Server-side tile serving
- Network tile streaming (local storage only)
- nlohmann/json style loading (hardcoded colors)
- Depth buffer
- Polygon holes
- Terrain elevation
- Windows, macOS, iOS support

---

## 2. System Architecture

### 2.1 Project Structure

```
map-renderer-v2/
├── engine/                         # Platform-independent rendering library
│   ├── CMakeLists.txt
│   ├── include/map_renderer/       # Public headers
│   │   ├── engine.h                # Main engine API
│   │   ├── camera.h                # 2D orthographic camera
│   │   ├── tile_id.h               # Tile coordinate types
│   │   ├── tile_cache.h            # LRU tile cache
│   │   ├── tile_loader.h           # Tile file I/O + protobuf deserialization
│   │   ├── renderer.h              # GL rendering (VAO/VBO/shaders)
│   │   ├── geometry_builder.h      # Geometry generation (triangulation, road quads)
│   │   ├── osm_types.h             # Internal data structures
│   │   ├── osm_loader.h            # Protobuf deserialization interface
│   │   ├── color_table.h           # Hardcoded feature colors
│   │   ├── platform.h              # Platform abstraction interface
│   │   ├── debug_log.h             # DEBUG_LOG macro
│   │   └── gl_check.h              # GL_CHECK macro
│   └── src/                        # Implementation
│       ├── camera.cpp
│       ├── tile_cache.cpp
│       ├── tile_loader.cpp
│       ├── renderer.cpp
│       ├── geometry_builder.cpp
│       ├── osm_loader.cpp          # Protobuf deserialization
│       ├── color_table.cpp
│       └── shaders/                # Embedded shader sources
│           ├── fill_vert.h         # GLSL source as C++ string
│           └── fill_frag.h
├── desktop_app/                    # SDL2 desktop application
│   ├── CMakeLists.txt
│   └── src/
│       └── main.cpp                # SDL2 window, GL 3.3 context, input, engine loop
├── android_app/                    # Android Studio project (separate)
│   ├── app/
│   │   ├── build.gradle
│   │   └── src/main/
│   │       ├── java/               # NativeActivity wrapper
│   │       ├── jni/                # JNI bridge to engine
│   │       └── AndroidManifest.xml
│   ├── build.gradle
│   └── settings.gradle
├── tools/                          # Python preprocessing
│   ├── preprocess.py               # PBF → tile pyramid
│   ├── osm_data.proto              # Tile protobuf schema
│   └── requirements.txt
├── data/                           # Data directory (gitignored)
│   ├── raw/                        # Source PBF files
│   ├── intermediate/               # Preprocessing intermediates
│   └── tiles/                      # Compiled tile output
│       ├── new_delhi/
│       └── india/
├── tests/                          # C++ unit + integration tests
│   ├── CMakeLists.txt
│   ├── test_camera.cpp
│   ├── test_geometry_builder.cpp
│   ├── test_tile_loader.cpp
│   ├── test_tile_cache.cpp
│   └── test_color_table.cpp
├── specs/                          # This specification
├── CMakeLists.txt                  # Top-level CMake
├── .gitignore
└── README.md
```

### 2.2 Component Overview

```
┌──────────────────────────────────────────────────────────────────┐
│                         Application Layer                         │
│  ┌──────────────────┐         ┌──────────────────┐               │
│  │  Desktop App     │         │  Android App     │               │
│  │  (SDL2 + GL 3.3) │         │  (NativeActivity │               │
│  │                  │         │   + EGL + GLES)  │               │
│  └────────┬─────────┘         └────────┬─────────┘               │
│           │    Platform Interface        │                         │
│           └──────────┬───────────────────┘                         │
└──────────────────────┼─────────────────────────────────────────────┘
                       │
┌──────────────────────┴─────────────────────────────────────────────┐
│                         Engine Library                             │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐         │
│  │ Camera   │  │ Tile     │  │ Tile     │  │ Renderer │         │
│  │ (2D)     │  │ Cache    │  │ Loader   │  │ (GLES)   │         │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘         │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                       │
│  │ Geometry │  │ OSM      │  │ Color    │                       │
│  │ Builder  │  │ Loader   │  │ Table    │                       │
│  └──────────┘  └──────────┘  └──────────┘                       │
└────────────────────────────────────────────────────────────────────┘
                       │
┌──────────────────────┴─────────────────────────────────────────────┐
│                      Preprocessing (Python)                        │
│  ┌──────────┐     ┌──────────┐     ┌──────────┐  ┌──────────┐    │
│  │ OSM PBF  │ ──→ │ Tile     │ ──→ │ Protobuf │→ │ zstd     │    │
│  │ Reader   │     │ Splitter │     │ Encoder  │  │ Compress │    │
│  │ (osmium) │     │ (z/x/y)  │     │          │  │          │    │
│  └──────────┘     └──────────┘     └──────────┘  └──────────┘    │
│                        │                                           │
│                   Douglas-Peucker                                   │
│                   Simplification                                    │
└─────────────────────────────────────────────────────────────────────┘
```

### 2.3 Layer Responsibilities

#### Application Layer
- Desktop: SDL2 window creation, GL 3.3 Core context, event polling, passes input to engine
- Android: NativeActivity lifecycle, EGL surface creation, GLES 3.0 context, touch input, passes to engine
- Both implement the platform abstraction interface

#### Engine Library
- Camera: 2D orthographic projection, pan/zoom, determines visible tiles
- Tile Cache: LRU cache of loaded tiles, thread-safe, evicts old tiles
- Tile Loader: Background thread, reads tile files, decompresses zstd, deserializes protobuf
- Renderer: GL state management, VAO/VBO management, shader program, draw calls
- Geometry Builder: Triangulates polygons, generates road quads from line strings
- OSM Loader: Protobuf deserialization into internal structures
- Color Table: Hardcoded feature → color mapping

#### Preprocessing (Python, offline)
- Reads OSM PBF using osmium
- Converts WGS84 lat/lon to per-tile local ENU meters
- Splits features into z/x/y tiles
- Applies Douglas-Peucker simplification per zoom level
- Encodes as protobuf, compresses with zstd
- Writes tile files to `data/tiles/<region>/z/x/y.bin`

---

## 3. Technology Stack

### 3.1 Core Technologies

| Component | Technology | Rationale |
|-----------|-----------|-----------|
| Language | C++17 | Broad Android NDK support, modern enough |
| Graphics | OpenGL ES 3.0 | Target device support, runs on all Android phones |
| Desktop GL | OpenGL 3.3 Core | Same API shape as GLES 3.0, easy development |
| GL Loader | GLAD | Generates only needed functions, CMake-friendly |
| Math | GLM | Header-only, trivial, needed for future 3D |
| Window (desktop) | SDL2 | Cross-platform, simple, input handling |
| Window (Android) | NativeActivity + EGL | Standard Android native rendering |
| Build (C++) | CMake 3.20+ | Standard, handles protobuf codegen + GLAD |
| Build (Android) | Gradle | Standard Android build system |
| Serialization | protobuf | Compact, C++/Python codegen, schema evolution |
| Compression | zstd | Best ratio/speed, small tile files |
| Tests | Google Test | Standard C++ test framework |
| Preprocessing | Python 3 + osmium | Fast OSM PBF reading, protobuf + zstd packages |

### 3.2 Dependencies Summary

**Build-time:**
- CMake 3.20+, C++17 compiler (GCC 12+ / Clang 15+ / NDK r25+)
- SDL2 dev (desktop only)
- protobuf + protoc
- GLAD (generated or FetchContent)
- GLM (FetchContent, header-only)
- Google Test (FetchContent)
- Android SDK + NDK (Android builds only)
- Python 3.8+ with osmium, protobuf, zstandard (preprocessing only)

**Runtime (desktop):**
- SDL2 shared lib
- OpenGL driver (system)

**Runtime (Android):**
- Native library (engine, statically linked)
- GLES 3.0 driver (system)
- No external runtime dependencies

### 3.3 What Was Dropped from v1.x

| Dropped | Replaced by |
|---------|-------------|
| Vulkan 1.2 | OpenGL ES 3.0 / GL 3.3 Core |
| C++23 | C++17 |
| 3D rendering, extrusion, lighting | 2D only (hooks kept) |
| Depth buffer | Draw order |
| nlohmann/json | Hardcoded color table |
| SPIR-V / glslc | Runtime GLSL compilation |
| Vulkan validation layers | GL_CHECK macro |
| Single monolithic data file | Tile pyramid (z/x/y directory) |
| Single executable | Engine library + thin apps |

---

## 4. Tiling System

### 4.1 Slippy Map Scheme

Standard Web Mercator tiling (same as OSM, Google Maps, MapLibre):
- Zoom level z, tile coordinates (x, y)
- At zoom z, the world is divided into 2^z × 2^z tiles
- Tile (0,0) is top-left (northwest)
- Tile size: 256×256 pixels at standard DPI (actual coverage in meters varies by latitude)

### 4.2 Zoom Levels (v2.0)

| Zoom | Coverage | Features Included |
|------|----------|-------------------|
| 8 | Country | Coastlines, major water, state boundaries, highways |
| 12 | Region/City | Major roads, parks, water, landmark buildings |
| 15 | Neighborhood | All roads, all parks/water/landuse, building clusters |
| 17 | Street | All features at full detail |

Architecture supports adding zoom levels between these (e.g., 10, 14, 16) without code changes — just preprocess more levels.

### 4.3 Coordinate System

**Coordinate systems:**

1. **Tile addressing:** Web Mercator (z/x/y) — standard, well-documented, deterministic tile bounds from lat/lon.

2. **Geometry storage:** Per-tile local ENU meters (float32) relative to the tile's center point.
   - `x` = east offset in meters from tile center
   - `z` = north offset in meters from tile center
   - `y` = up (reserved for future 3D, always 0 in 2D)

3. **World space:** Global ENU meters from a dataset-wide reference point (stored in metadata file).
   - Each tile stores its center as lat/lon (double) in the protobuf header
   - The OSMLoader converts tile center lat/lon to a global ENU offset during deserialization
   - Vertex shader computes: `world_pos = tile_offset + local_pos`

**Why per-tile local ENU?**
- Float32 precision is perfect (tile spans are small, typically < 50 km)
- Physically accurate meters (buildings, roads at correct scale)
- Future 3D just extrudes `y` (heights already in protobuf schema)
- No latitude-dependent scale distortion within a tile

### 4.4 Tile File Format

```
data/tiles/<region>/<z>/<x>/<y>.bin
```

Each file is a zstd-compressed protobuf message (see LLD for schema).

**Metadata file:**
```
data/tiles/<region>/metadata.bin
```
A zstd-compressed protobuf `DatasetMetadata` message (see LLD §3.2). Contains: dataset name, bounds (min/max lat/lon), reference origin (lat/lon), zoom levels present, total tile count. Using protobuf (not JSON) avoids a runtime JSON parser dependency. Small (~100 bytes).

### 4.5 Tile Loading Strategy

```
1. Camera computes visible tile range from viewport (only when dirty)
2. Engine requests tiles from TileLoader (visible + 1-ring prefetch)
3. TileLoader thread loads missing tiles: file → zstd → protobuf → TileCache
4. Render thread draws with whatever tiles are ready (stale LOD or blank ok)
5. Each frame, Engine polls TileCache.drain_recent_inserts() to learn which
   tiles finished loading, then uploads their geometry to GPU on the render
   thread and frees the CPU feature vectors
6. Eviction: LRU, when cache exceeds max size (frees GPU buffers via callback)
```

### 4.6 Simplification

Douglas-Peucker simplification tolerance per zoom level:

| Zoom | Tolerance | Rationale |
|------|-----------|-----------|
| 8 | 500 m | Country view, coarse |
| 12 | 50 m | City view, moderate |
| 15 | 5 m | Neighborhood, fine |
| 17 | 0.5 m | Street view, near-full detail |

Applied to: road line strings, polygon boundaries. Buildings: skipped at zoom < 15 (too small to see).

---

## 5. Platform Abstraction

### 5.1 Interface

The engine defines a `PlatformInterface` that each app implements:

```
PlatformInterface:
  - GL function pointers (loaded by app, passed to engine)
  - get_viewport_size() → (width, height)
  - get_tile_data_path() → filesystem path to tiles
  - input callbacks: pan, zoom, quit (pushed by app into engine)
```

The engine never calls SDL2, EGL, or Android APIs directly. All platform interaction goes through this interface.

### 5.2 Desktop Implementation

- SDL2 creates window, GL 3.3 Core context
- GLAD loads GL functions in the app, pointers passed to engine
- SDL events translated to input callbacks, pushed to engine
- Tile path: `data/tiles/<region>/` (configurable via command line)

### 5.3 Android Implementation

- NativeActivity handles lifecycle
- EGL creates GLES 3.0 surface
- GLAD (GLES mode) loads functions in JNI bridge
- Touch events (MotionEvent) translated to pan/zoom, pushed to engine
- Tile path: app internal storage or external storage (configurable)

---

## 6. Rendering Pipeline

### 6.1 Single Shader Program

One shader program for all 2D geometry:
- Vertex shader: transforms local coords + tile offset by camera matrix
- Fragment shader: outputs color from uniform

Feature color is set via `glUniform4f` before each draw call. No per-vertex color attribute — saves VBO space.

### 6.2 Draw Order (Painter's Algorithm)

```
1. Clear (solid ground color)
2. For each visible tile:
   a. Draw water polygons (blue)
   b. Draw park polygons (green)
   c. Draw landuse polygons (tan)
   d. Draw road quads (white for v2.0; per-type colors reserved for future)
   e. Draw building footprints (tan)
```

No depth buffer. Draw order guarantees correct visual layering.

### 6.3 VBO Layout

Each tile gets one VBO containing all geometry:
- Water polygon vertices (triangulated)
- Park polygon vertices (triangulated)
- Landuse polygon vertices (triangulated)
- Road quad vertices
- Building footprint vertices (triangulated)

Offsets and counts stored per tile. VAO per tile (each TileData owns its VAO + VBO).

### 6.4 Render Loop

```
each frame (Engine::update):
  1. Apply input events to camera (set dirty flag if moved)
  2. If camera dirty:
     a. Recompute view/projection matrices (cached)
     b. Recompute visible tile range + 1-ring prefetch (cached vector)
     c. Request prefetch tiles from TileLoader; cancel the rest
  3. Drain newly-loaded tiles from cache:
     a. For each: build geometry, upload VBO to GPU (render thread)
     b. Free CPU feature vectors (memory reclamation)
  4. Render:
     a. Clear (ground color)
     b. Bind shader, set camera matrix uniforms
     c. For each tile in cached visible_tiles_:
        - Set tile offset uniform, bind VAO
        - For each feature range: set color uniform, glDrawArrays
  5. Swap buffers (VSync blocks)
```

**Zero allocations in the hot path.** visible_tiles_ is reused (cleared + refilled only when camera dirty). Camera matrices reused. Uniform locations cached at shader load time. GLFunctions uses standard C++ types (no GL headers in engine core).

---

## 7. Camera System

### 7.1 2D Orthographic Camera

- Position: (x, z) in world ENU meters
- Zoom: maps to visible world span (meters across viewport)
- Pan: drag or arrow keys, speed proportional to visible span
- Zoom: scroll/pinch, adjusts visible span

### 7.2 Tile Zoom Selection

Camera visible span → nearest tile zoom level:

| Visible span (m) | Tile zoom |
|-------------------|-----------|
| > 500,000 | 8 |
| > 50,000 | 12 |
| > 5,000 | 15 |
| ≤ 5,000 | 17 |

When crossing a threshold, the engine requests tiles from the new zoom level. The old level remains visible until the new level is loaded (graceful degradation).

### 7.3 Future 3D Hook

`CameraMode` enum with `MODE_2D` and `MODE_3D`. Only `MODE_2D` implemented. The `MODE_3D` value is reserved. Camera class stores position in a way that 3D perspective can be added without changing the interface.

---

## 8. Performance Strategy

### 8.1 Frame Rate
- VSync caps at display refresh rate (60-144 Hz on Pad 6)
- Zero allocations in render loop
- Matrix recomputation only on camera change (dirty flag)
- Single shader program (no program switches)
- 5 draw calls per tile (one per feature type)

### 8.2 Memory
- LRU tile cache (default 64 tiles, configurable)
- TileData CPU feature vectors (`buildings`, `roads`, `polygons`) freed
  after GPU upload — only the ~40-byte shell (id, ranges, GPU handles)
  stays cached (LLD §8.4)
- Protobuf vectors reserved from header counts
- No per-frame heap allocations (visible_tiles_ reused)
- GLM uses stack-local matrices (no heap)

### 8.3 Storage
- zstd compression on tile files
- Empty tiles not generated
- Float32 coordinates (not double)
- Douglas-Peucker simplification at low zoom
- Buildings skipped at low zoom (saves tile size)

### 8.4 CPU
- Background thread for tile I/O (render thread never blocks)
- No JSON parsing at runtime (hardcoded colors)
- Cached uniform locations (no string lookups per frame)

---

## 9. Development Phases

### Phase 1: Engine Foundation (Week 1)
- CMake build system for engine library + desktop app
- Platform abstraction interface
- GLAD integration, GL_CHECK macro
- DEBUG_LOG macro
- Google Test setup
- Headless test harness

### Phase 2: Data Pipeline (Week 1-2)
- Protobuf tile schema
- Python preprocessor (PBF → tiles with simplification + zstd)
- Tile loader (zstd decompress + protobuf deserialize)
- OSM types and internal structures
- Unit tests for loader

### Phase 3: Rendering Core (Week 2-3)
- Single shader program (embedded GLSL, ES-compatible)
- VAO/VBO management
- Geometry builder (polygon triangulation, road quads)
- Color table
- Renderer: draw tiles with correct order
- Desktop app: SDL2 window + GL 3.3 context + render loop

### Phase 4: Tiling + Camera (Week 3-4)
- Tile cache (LRU, thread-safe)
- Background tile loading thread
- Camera (2D orthographic, pan/zoom, dirty flag)
- Visible tile range computation
- Tile zoom selection
- Prefetch ring
- Integration: camera drives tile loading drives rendering

### Phase 5: Android App (Week 4-5)
- Android Studio project setup
- NativeActivity + EGL + GLES 3.0
- JNI bridge to engine library
- Touch input (pan + pinch zoom)
- Tile data deployment to device
- Test on Xiaomi Pad 6

### Phase 6: Testing + Optimization (Week 5-6)
- Full unit + integration test suite
- ASan/LSan runs
- Performance profiling on desktop and device
- Tile cache tuning
- India dataset benchmark
- Documentation

---

## 10. Risks and Mitigations

### 10.1 Technical Risks

| Risk | Mitigation |
|------|------------|
| GLES 3.0 shader incompatibility with desktop GL 3.3 | Write shaders with `#version 300 es` precision qualifiers that also compile under `#version 330 core`; test both early |
| Tile loading stalls on slow Android storage | Background thread + LRU cache + prefetch ring; render with stale LOD if needed |
| zstd not available on Android NDK | Statically link zstd library into the engine; it's small (~200 KB) |
| Protobuf on Android NDK | Cross-compile protobuf C++ runtime with NDK toolchain; statically link |
| India preprocessing takes very long | Run overnight; process zoom levels independently; can parallelize per tile |
| Memory pressure on device | LRU cache with hard cap; free CPU data after GPU upload; monitor with Android Profiler |
| Float precision at tile edges | Per-tile local coordinates (spans < 50 km), float32 is plenty |

### 10.2 Project Risks

| Risk | Mitigation |
|------|------------|
| Scope creep (adding 3D early) | 3D is explicitly v3.0; do not implement until 2D is complete and tested |
| Android build complexity | Keep JNI bridge minimal; engine is a pure C++ library; Android just provides EGL surface + input |
| Preprocessing correctness | Unit test ENU conversion, simplification, and protobuf encoding with known data |

---

## 11. Resolved Decisions

1. **Graphics API:** OpenGL ES 3.0 (develop on GL 3.3 Core)
2. **Language:** C++17
3. **Tiling:** Slippy map (z/x/y, Web Mercator)
4. **Zoom levels:** 8, 12, 15, 17 (extensible)
5. **Tile storage:** Directory of zstd-compressed protobuf files
6. **Coordinates:** Per-tile local ENU meters (float32)
7. **Compression:** zstd
8. **GL loader:** GLAD
9. **Colors:** Hardcoded table (no JSON for v2.0)
10. **Shaders:** Single program, embedded as C++ strings
11. **Depth buffer:** None (draw order)
12. **Roads:** Quads (not GL lines — portability)
13. **Architecture:** Engine library + thin desktop/Android apps
14. **Platform abstraction:** Interface in engine, implemented by each app
15. **3D:** Deferred to v3.0, hooks in Camera and data schema
16. **Target device:** Xiaomi Pad 6 (Android 13)
17. **Test data:** New Delhi (35 MB PBF)
18. **Benchmark data:** India (1.6 GB PBF)

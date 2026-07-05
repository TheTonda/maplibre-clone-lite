# Project Status

**Updated:** 2026-07-05

## Summary

Interactive 2D Slippy-Map Renderer — C++17, OpenGL ES 3.0, targeting Android (Xiaomi Pad 6).
17 of 28 tasks complete (Phases 1–4 done). Phase 5 (Android) is next.

## Phases

| Phase | Tasks | Status |
|-------|-------|--------|
| 1 — Engine Foundation | 1–4 | ✅ complete |
| 2 — Data Pipeline | 5–7 | ✅ complete |
| 3 — Rendering Core | 8–12 | ✅ complete |
| 4 — Tiling + Camera | 13–17 | ✅ complete |
| 5 — Android App | 18–22 | 🔴 pending |
| 6 — Testing + Optimization | 23–28 | 🔴 pending |

## What Works

- **Desktop app** (SDL2 + OpenGL 3.3): window, interactive pan/zoom, tile streaming
- **Engine**: orchestrator loop with camera → visible tiles → loader → renderer
- **Camera**: 2D orthographic, pan clamped to dataset bounds, zoom, aspect-aware mats
- **Tile system**: slippy-map z/x/y addressing (zoom 8/12/15/17), LRU cache, background loader thread
- **Renderer**: VAO/VBO per tile, single-shader program, draw-order (water→park→landuse→road→building)
- **Data pipeline**: OSM PBF → Python preprocessor → zstd-compressed protobuf tiles in per-tile ENU meters
- **Tests**: 11/11 passing (platform, types, color table, geometry builder, shaders, renderer, camera, tile cache, engine, OSM loader)

## Directory Structure

```
map-renderer-v2/
├── CMakeLists.txt          # Top-level build (C++17, GLM, protobuf, zstd, SDL2, GLAD, GTest)
├── TASKS.md                # Implementation checklist (17/28 done)
├── STATUS.md               # This file
├── engine/                 # Core library (platform-independent)
│   ├── include/map_renderer/
│   │   ├── platform.h      # PlatformInterface + GLFunctions
│   │   ├── tile_id.h       # TileId, hash
│   │   ├── osm_types.h     # Point, Building, Road, PolygonFeature, TileData
│   │   ├── color_table.h   # Feature → color map
│   │   ├── geometry_builder.h
│   │   ├── shader.h        # GLSL strings + program wrapper
│   │   ├── renderer.h      # VAO/VBO, draw loop
│   │   ├── osm_loader.h    # Protobuf → TileData
│   │   ├── tile_loader.h   # File I/O + zstd + background thread
│   │   ├── tile_cache.h    # Thread-safe LRU cache
│   │   ├── camera.h        # 2D orthographic, pan/zoom
│   │   └── engine.h        # Orchestrator (init/update/shutdown)
│   └── src/                # Implementation files
├── desktop_app/            # SDL2 desktop app
│   └── src/main.cpp
├── tools/                  # Python preprocessor
│   ├── preprocess.py
│   ├── osm_data.proto
│   └── requirements.txt
├── tests/                  # GTest-based unit + integration tests
├── specs/                  # Requirements, HLD, LLD, TASKS
│   ├── REQUIREMENTS.md
│   ├── HLD.md
│   ├── LLD.md
│   └── TASKS.md
├── data/                   # Preprocessed tiles (gitignored)
├── screenshots/            # Dev screenshots (gitignored)
└── traces/                 # Apitrace captures (gitignored)
```

## Current Commit Changes

This commit (post-Task 17 cleanup):

- **Build system**: Switched protobuf discovery from `find_package(absl)` to pkg-config (`PkgConfig::PROTOBUF`), which correctly pulls in protobuf's transitive dependencies (abseil, etc.). Removed all manual `absl::*` linkage from desktop_app and tests.
- **Camera fixes**: Pan now calls clamp; clamp guards against unset bounds with a `bounds_set_` flag; aspect ratio change detected in `get_projection_matrix()`; `get_view_matrix()` reuses stored aspect.
- **Engine startup zoom**: After `frame_dataset()`, engine zooms in until `visible_span ≤ 2000m` so roads are visible at launch (New Delhi dataset is ~50km — roads would be sub-pixel at full-span).
- **Geometry builder**: Minor formatting fix.

## Build & Test

```
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
cd build && ctest          # 11/11 passing
```

Dependencies: CMake 3.16+, C++17 compiler, protobuf (pkg-config), zstd, SDL2, GLAD (FetchContent), GLM (FetchContent), GTest (FetchContent).

## Next: Phase 5 — Android

1. **Task 18**: Android Studio project setup (Gradle, NDK, cross-compile deps)
2. **Task 19**: Android platform (EGL + GLES 3.0, lifecycle)
3. **Task 20**: Touch input (single-finger pan, two-finger pinch zoom)
4. **Task 21**: Tile data deployment to device
5. **Task 22**: Full render test on Xiaomi Pad 6

## Known Issues / Technical Debt

- No `.gitignore` entries for `screenshots/` and `traces/` — currently untracked; should decide whether to add to .gitignore.
- Android cross-compilation of protobuf and zstd not yet tested.
- Shader embeds `#version 330 core` — needs `#ifdef` switch to `#version 300 es` for GLES.
- No continuous integration.

# Detailed Task Breakdown
## Interactive 2D Map Renderer

**Version:** 2.0  
**Date:** July 3, 2026  
**Status:** Specification — Redesigned for 2D tiled rendering on Android

---

## Overview

This document breaks down the project into detailed tasks and subtasks. Each task is small enough to be completed in 1-4 hours and has clear acceptance criteria.

**Total Tasks:** 28  
**Estimated Effort:** 5 to 6 weeks

---

## Phase 1: Engine Foundation (Week 1)

### Task 1: CMake build system + project structure
**Priority:** Critical  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] Create top-level CMakeLists.txt with engine library + desktop_app + tests targets
- [ ] Set C++17, -Wall -Wextra -Wpedantic
- [ ] Create directory structure: engine/include, engine/src, desktop_app, android_app, tools, data, tests
- [ ] FetchContent GLM
- [ ] FetchContent Google Test
- [ ] Add GLAD via FetchContent or generate and vendor
- [ ] Add protobuf find_package + protoc integration
- [ ] Create .gitignore (already done — verify)
- [ ] Create empty data/raw, data/intermediate, data/tiles directories with .gitkeep
- [ ] Verify empty project builds: `cmake -B build && cmake --build build`

**Acceptance Criteria:**
- `cmake ..` succeeds
- `make` compiles without errors
- All dependencies fetched or found
- `protoc` is available

---

### Task 2: Platform abstraction interface
**Priority:** Critical  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] Create `platform.h` with `GLFunctions` struct, `PlatformInterface` class, `InputEvent` enum, `InputData` struct
- [ ] Create `debug_log.h` with `DEBUG_LOG` macro (gated by `MAP_RENDERER_DEBUG`)
- [ ] Create `gl_check.h` with `GL_CHECK` macro (gated by `MAP_RENDERER_DEBUG`)
- [ ] Write test verifying macros compile in both debug and release modes

**Acceptance Criteria:**
- PlatformInterface is a pure virtual class
- DEBUG_LOG compiles to nothing in release
- GL_CHECK compiles to nothing in release
- No platform-specific headers in engine/include

---

### Task 3: Tile ID + OSM types
**Priority:** Critical  
**Estimated Time:** 1 hour

**Subtasks:**
- [ ] Create `tile_id.h` with TileId struct, operator==, Hash
- [ ] Create `osm_types.h` with Point, Building, Road, PolygonFeature, TileData
- [ ] Verify TileId::Hash works in std::unordered_map

**Acceptance Criteria:**
- TileId is trivially copyable
- Hash function works for z up to 20, x/y up to 2^20

---

### Task 4: Color table
**Priority:** High  
**Estimated Time:** 1 hour

**Subtasks:**
- [ ] Create `color_table.h` with hardcoded feature → color map
- [ ] Include colors for: ground, water, park, landuse, building, road, road_primary, road_secondary
- [ ] Unknown type returns magenta (debug indicator)
- [ ] Write unit tests

**Acceptance Criteria:**
- All known feature types return correct colors
- Unknown types return magenta
- No external dependencies (no JSON)

---

## Phase 2: Data Pipeline (Week 1-2)

### Task 5: Protobuf tile schema
**Priority:** Critical  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] Create `tools/osm_data.proto` with Point2D, Building, Road, PolygonFeature, Tile, DatasetMetadata
- [ ] Add CMake rule to generate C++ sources from .proto
- [ ] Verify generated C++ compiles
- [ ] Generate Python module (protoc --python_out) for preprocessor
- [ ] Verify Python module imports

**Acceptance Criteria:**
- .proto file defines all messages
- C++ generated code compiles
- Python generated code imports

---

### Task 6: Python preprocessor
**Priority:** Critical  
**Estimated Time:** 5 hours

**Subtasks:**
- [ ] Create `tools/requirements.txt` (osmium, protobuf, zstandard)
- [ ] Create `tools/preprocess.py` with CLI: input PBF, output dir, zoom levels
- [ ] Implement PBF reading with osmium
- [ ] Implement dataset bounds computation
- [ ] Implement WGS84 → per-tile local ENU conversion
- [ ] Implement tile assignment (feature → z/x/y tiles)
- [ ] Implement Douglas-Peucker simplification (per zoom tolerance)
- [ ] Skip buildings at zoom < 15
- [ ] Implement protobuf serialization
- [ ] Implement zstd compression
- [ ] Write tile files to `output_dir/z/x/y.bin`
- [ ] Write metadata.bin (zstd-compressed protobuf DatasetMetadata)
- [ ] Test with NewDelhi.osm.pbf

**Acceptance Criteria:**
- NewDelhi.osm.pbf → tiles/new_delhi/z/x/y.bin
- Tiles are valid zstd-compressed protobuf
- Coordinates are in local ENU meters
- Low-zoom tiles have simplified geometry
- Empty tiles not generated
- metadata.bin contains bounds and reference point

---

### Task 7: OSM loader + tile loader (C++)
**Priority:** Critical  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] Create `osm_loader.h` with OSMLoader class (pure protobuf deserialize)
- [ ] Create `tile_loader.h` with TileLoader class (file I/O + zstd + thread)
- [ ] Implement file path computation: `tile_dir/z/x/y.bin`
- [ ] Implement zstd decompression (link zstd library)
- [ ] Implement `OSMLoader::deserialize()` — protobuf bytes → TileData,
      reserve vectors from header counts, compute world offset from
      center_lat/lon using the §5.3 ENU formula
- [ ] Implement `TileLoader::load_tile()` — read file → zstd → call
      OSMLoader::deserialize → hand to TileCache::put
- [ ] Handle missing file (return false, log warning)
- [ ] Handle corrupt file (return false, log error)
- [ ] Write unit tests for OSMLoader (with a small test protobuf buffer)
- [ ] Write unit tests for TileLoader file path + missing-file handling

**Acceptance Criteria:**
- OSMLoader deserializes a protobuf buffer → correct TileData
- TileLoader loads a tile file → correct TileData in cache
- Missing file → graceful failure (no crash)
- Vectors pre-allocated (no reallocation during parse)
- World offset computed with the exact §5.3 formula

---

## Phase 3: Rendering Core (Week 2-3)

### Task 8: Geometry builder
**Priority:** Critical  
**Estimated Time:** 4 hours

**Subtasks:**
- [ ] Create `geometry_builder.h` with BuiltGeometry struct, GeometryBuilder class
- [ ] Implement ear-clipping triangulation (convex + concave polygons)
- [ ] Implement winding order validation (ensure CCW)
- [ ] Implement road quad generation (perpendicular extrusion)
- [ ] Implement `build_tile()` — builds all geometry for a tile, returns vertices + draw ranges
- [ ] Handle empty features (zero vertices for missing types)
- [ ] Write unit tests: convex polygon, concave polygon, degenerate polygon, road quad, multi-segment road

**Acceptance Criteria:**
- Convex polygon triangulates correctly
- Concave polygon triangulates correctly
- Road quads have correct width and orientation
- Draw ranges are correct offsets/counts
- No heap allocations after build_tile returns

---

### Task 9: Shader program
**Priority:** Critical  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] Create `shaders/fill_vert.h` with GLSL vertex shader as raw string literal
- [ ] Create `shaders/fill_frag.h` with GLSL fragment shader as raw string literal
- [ ] Ensure shaders are ES 3.0 compatible (precision qualifiers, no deprecated features)
- [ ] Implement shader compilation in Renderer (compile vertex + fragment, link program)
- [ ] Cache uniform locations (proj, view, color, tile_offset)
- [ ] Log compilation/linking errors in debug builds
- [ ] Verify shader compiles under both `#version 330 core` and `#version 300 es`

**Acceptance Criteria:**
- Shader program compiles and links
- Uniform locations cached
- Error logging works in debug mode
- GL_CHECK passes after compilation

---

### Task 10: Renderer — GL initialization + tile upload
**Priority:** Critical  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] Implement `Renderer::initialize()` — store GL functions, compile shader, get uniform locations
- [ ] Implement `upload_tile_geometry()` — build geometry, create VAO + VBO, upload vertices
- [ ] Implement `on_tile_loaded()` callback — called by TileLoader, uploads geometry to GPU
- [ ] Implement `on_tile_evicted()` callback — called by TileCache, deletes VAO + VBO
- [ ] Implement `Renderer::cleanup()` — delete shader program, all VAOs/VBOs
- [ ] GL_CHECK after every GL call in debug builds

**Acceptance Criteria:**
- Tile geometry uploaded to GPU (VAO + VBO created)
- GL resources freed on eviction and cleanup
- No GL errors in debug mode
- VBO contains all feature vertices in draw order

---

### Task 11: Renderer — draw loop
**Priority:** Critical  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] Implement `Renderer::render()` — clear, bind shader, set camera uniforms, iterate tiles
- [ ] Per tile: set tile_offset uniform, bind VAO, draw each feature range with color uniform
- [ ] Draw order: water → park → landuse → road → building
- [ ] Skip tiles not in cache (not loaded yet)
- [ ] GL_CHECK at end of frame
- [ ] Zero allocations in render loop

**Acceptance Criteria:**
- Tiles render with correct colors and draw order
- Missing tiles don't crash (just skipped)
- No per-frame heap allocations
- GL_CHECK clean

---

### Task 12: Desktop app — window + single-tile render (SDL2)
**Priority:** Critical  
**Estimated Time:** 3 hours

**Note:** This task delivers a working window that renders one hardcoded
tile, and collects input events. Pan/zoom/camera integration is deferred to
Task 17 (Engine orchestrator), which wires the Camera (Task 13) into the
loop. Do NOT implement camera logic here.

**Subtasks:**
- [ ] Create `desktop_app/CMakeLists.txt` (link engine, SDL2, GLAD)
- [ ] Create `desktop_app/src/main.cpp`
- [ ] Implement DesktopPlatform class (implements PlatformInterface)
- [ ] SDL2 init: window, GL 3.3 Core context, VSync
- [ ] GLAD init: load GL functions, fill GLFunctions struct
- [ ] Event loop: collect SDL events into an InputData vector (no camera
      application yet — just translation)
- [ ] Map: mouse drag → PanMove, scroll → Zoom, ESC → KeyQuit,
      +/- → KeyZoomIn/Out, arrows → KeyPan*
- [ ] Manually load ONE test tile via TileLoader/OSMLoader and render it
      with the Renderer to verify the full GL path works end-to-end
- [ ] Tile path from command line argument

**Acceptance Criteria:**
- Window opens and shows the clear color (or one test tile if available)
- ESC quits cleanly
- Input events are collected and translated (camera not yet applied)
- No GL errors
- VSync active (CPU not at 100% when idle)
- Pan/zoom are wired in Task 17, not here

---

## Phase 4: Tiling + Camera (Week 3-4)

### Task 13: Camera (2D orthographic)
**Priority:** Critical  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] Create `camera.h/.cpp` with Camera class
- [ ] Implement orthographic projection (visible_span → ortho matrix)
- [ ] Implement pan (clamp to dataset bounds)
- [ ] Implement zoom (clamp visible_span to valid range)
- [ ] Implement dirty flag (matrices recomputed only on change)
- [ ] Implement `get_projection_matrix()` and `get_view_matrix()` (cached)
- [ ] Implement `apply_input()` — process InputData events
- [ ] Implement `set_dataset_bounds()` and `frame_dataset()`
- [ ] Write unit tests: matrix correctness, pan/zoom clamping, dirty flag behavior

**Acceptance Criteria:**
- Ortho matrix correct for given span and aspect
- Pan/zoom clamped to valid ranges
- Matrices only recomputed when dirty
- Dirty flag set on any state change
- frame_dataset() centers and fits bounds

---

### Task 14: Tile zoom selection + visible tile computation
**Priority:** Critical  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] Implement `get_tile_zoom()` — visible_span → nearest zoom level (8/12/15/17)
- [ ] Implement world ENU → WGS84 lat/lon conversion (inverse of preprocessor)
- [ ] Implement lat/lon → tile x/y (standard slippy map formula)
- [ ] Implement `get_visible_tiles()` — compute tile range from viewport bounds
- [ ] Handle edge cases: viewport spanning antimeridian (not needed for India, but code defensively)
- [ ] Write unit tests: known camera positions → expected tile ranges

**Acceptance Criteria:**
- Correct tile zoom for given visible span
- Visible tile list covers the viewport
- No duplicate tiles in list
- Tiles outside viewport not included

---

### Task 15: Tile cache (LRU, thread-safe)
**Priority:** Critical  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] Create `tile_cache.h/.cpp` with TileCache class
- [ ] Implement LRU eviction (std::list + unordered_map)
- [ ] Implement thread-safe get/put (mutex)
- [ ] Implement eviction callback (for GPU resource cleanup)
- [ ] Implement `invalidate()` — remove specific tile
- [ ] Write unit tests: fill cache, verify eviction order, verify thread safety (multi-threaded test)

**Acceptance Criteria:**
- Cache evicts least-recently-used when full
- get() marks tile as recently used
- No race conditions under concurrent access
- Eviction callback fires before tile is removed

---

### Task 16: Background tile loading thread
**Priority:** High  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] Implement TileLoader worker thread
- [ ] Implement request queue (mutex + condition variable)
- [ ] Implement `request_tiles()` — add to queue if not already pending
- [ ] Implement `cancel_tiles()` — remove from pending set
- [ ] Worker loop: pop from queue, load tile, hand to cache
- [ ] Handle duplicate requests (don't load same tile twice)
- [ ] Graceful shutdown (stop flag, join thread)
- [ ] Write integration test: request tiles, verify they appear in cache

**Acceptance Criteria:**
- Render thread never blocks on I/O
- Requested tiles load in background
- Cancelled tiles not loaded (if not already loading)
- No deadlocks
- Clean shutdown (thread joins)

---

### Task 17: Engine orchestrator + desktop app integration
**Priority:** Critical  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] Create `engine.h/.cpp` with Engine class
- [ ] Implement `initialize()` — load metadata.bin (zstd + protobuf),
      create camera/cache/loader/renderer, set eviction callback
- [ ] Implement `update()` per LLD §8.2:
      input → camera → if dirty recompute visible+prefetch (§8.3) →
      request/cancel tiles → drain_recent_inserts + upload +
      free CPU vectors (§8.4) → render(visible_tiles_)
- [ ] Implement `recompute_visible_tiles()` — core visible + 1-ring prefetch
- [ ] Implement `drain_pending_uploads()` — poll cache, call
      renderer.on_tile_loaded, free CPU feature vectors after upload
- [ ] Implement `on_resize()` — update viewport
- [ ] Implement `shutdown()` — stop loader, cleanup renderer, clear cache
- [ ] Load dataset metadata (ref_lat, ref_lon, bounds) from metadata.bin
- [ ] Frame dataset on init (camera.frame_dataset())
- [ ] Wire desktop_app/main.cpp to call Engine::update() each frame with
      the collected InputData — this completes the interactive pan/zoom
      loop deferred from Task 12
- [ ] Verify mouse drag pans, scroll zooms (camera now wired)
- [ ] Write integration test: initialize with test data, verify no crash

**Acceptance Criteria:**
- Engine initializes and runs without crash
- Desktop app: mouse drag pans, scroll zooms (Task 12 loop completed here)
- Camera movement triggers tile loading (visible + prefetch ring)
- Tiles appear as they load (no frame stall — uploads on render thread)
- CPU feature vectors freed after GPU upload (memory stays low)
- Clean shutdown with no resource leaks

---

## Phase 5: Android App (Week 4-5)

### Task 18: Android Studio project setup
**Priority:** Critical  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] Create `android_app/` directory with Android Studio project structure
- [ ] Configure Gradle: minSdk 24, targetSdk 33, NDK r25+
- [ ] Configure CMakeLists.txt for native library (link engine, GLES, EGL, log, zstd, protobuf)
- [ ] Cross-compile protobuf C++ runtime for Android NDK (or use lite runtime)
- [ ] Cross-compile zstd for Android NDK
- [ ] Add engine source to native build (or link prebuilt static lib)
- [ ] Create AndroidManifest.xml with NativeActivity
- [ ] Verify empty native lib builds and loads on device

**Acceptance Criteria:**
- Android Studio project opens without errors
- Native library builds with NDK
- App installs and launches on Xiaomi Pad 6
- Logcat shows native lib loaded

---

### Task 19: Android platform implementation (EGL + GLES)
**Priority:** Critical  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] Create `android_platform.cpp` — implements PlatformInterface
- [ ] EGL: choose config, create context (GLES 3.0), create surface from ANativeWindow
- [ ] GLAD: load GLES 3.0 functions, fill GLFunctions struct
- [ ] Handle app lifecycle: pause/resume (release/recreate EGL surface)
- [ ] Handle window resize (recreate EGL surface)
- [ ] Implement viewport size from ANativeWindow dimensions
- [ ] Implement tile data path (app internal storage)
- [ ] Implement VSync (eglSwapInterval(1))

**Acceptance Criteria:**
- GLES 3.0 context created successfully
- GL functions loaded via GLAD
- Clear color visible on screen
- Lifecycle handled (no crash on pause/resume)

---

### Task 20: Android touch input
**Priority:** Critical  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] Handle MotionEvent in NativeActivity
- [ ] Single-finger drag → PanMove (screen delta)
- [ ] Two-finger pinch → Zoom (distance ratio delta)
- [ ] Pass touch events to Engine as InputData
- [ ] Account for display density (px → dp if needed, though engine works in world meters)
- [ ] Test panning and zooming on device

**Acceptance Criteria:**
- Single-finger drag pans smoothly
- Two-finger pinch zooms proportionally
- No jitter or lag
- Multi-touch correctly tracked

---

### Task 21: Tile data deployment to device
**Priority:** High  
**Estimated Time:** 1 hour

**Subtasks:**
- [ ] Preprocess New Delhi tiles on desktop
- [ ] Copy tiles to device via adb push to app internal storage
- [ ] Or: bundle tiles in APK assets (if small enough)
- [ ] Configure app to read from correct path
- [ ] Verify engine loads tiles from device storage

**Acceptance Criteria:**
- Tiles accessible on device
- Engine loads and renders them
- No file permission errors

---

### Task 22: Android app — full render test
**Priority:** Critical  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] Run app on Xiaomi Pad 6 with New Delhi tiles
- [ ] Verify map renders correctly
- [ ] Verify pan and zoom work
- [ ] Verify tile loading (new tiles load when panning)
- [ ] Check FPS with Android Studio Profiler or FPS overlay
- [ ] Check memory usage with Android Profiler
- [ ] Fix any device-specific issues

**Acceptance Criteria:**
- Map renders on Xiaomi Pad 6
- Pan and zoom smooth (60 FPS target)
- Memory < 150 MB
- No crashes during extended use
- Tile loading doesn't stall frames

---

## Phase 6: Testing + Optimization (Week 5-6)

### Task 23: Full test suite
**Priority:** High  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] Complete all unit tests (camera, geometry, tile loader, tile cache, color table)
- [ ] Integration test: load metadata → frame → render (mock GL)
- [ ] Integration test: camera move → tile request → cache update
- [ ] Run all tests under AddressSanitizer + LeakSanitizer
- [ ] Fix any memory errors
- [ ] Verify 100% test pass rate

**Acceptance Criteria:**
- All tests pass
- ASan/LSan clean (no errors)
- No memory leaks

---

### Task 24: Performance profiling + optimization
**Priority:** High  
**Estimated Time:** 4 hours

**Subtasks:**
- [ ] Profile desktop app with perf or vtune
- [ ] Profile Android app with Android Studio Profiler (CPU + GPU)
- [ ] Identify hot spots in render loop
- [ ] Verify zero per-frame allocations (no malloc in hot path)
- [ ] Tune tile cache size (balance memory vs. tile reloads)
- [ ] Optimize geometry builder if slow (measure tiles/sec)
- [ ] Optimize protobuf deserialization if slow
- [ ] Verify VSync is active (CPU usage low when idle)

**Acceptance Criteria:**
- 60 FPS on desktop with New Delhi
- 60 FPS on Xiaomi Pad 6 with New Delhi
- No frame stalls > 16 ms during tile loading
- CPU usage < 30% when idle (VSync)
- Memory < 150 MB

---

### Task 25: India dataset benchmark
**Priority:** High  
**Estimated Time:** 3 hours

**Subtasks:**
- [ ] Preprocess india-260623.osm.pbf (may take significant time)
- [ ] Copy a subset of tiles to device (e.g., one state)
- [ ] Run desktop app with India tiles at various zoom levels
- [ ] Run Android app with India tiles on Xiaomi Pad 6
- [ ] Measure FPS at zoom 8, 12, 15, 17
- [ ] Measure memory at each zoom level
- [ ] Verify tile loading doesn't cause OOM
- [ ] Verify LRU eviction works (memory stable over time)

**Acceptance Criteria:**
- India tiles render on both desktop and Android
- 40+ FPS at all zoom levels on Xiaomi Pad 6
- Memory stable over time (LRU working)
- No OOM on device
- Panning across large areas doesn't stall

---

### Task 26: Documentation
**Priority:** Medium  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] Update README.md with build instructions (desktop + Android)
- [ ] Document preprocessor usage
- [ ] Document engine API (public headers)
- [ ] Document Android app setup (device, tile deployment)
- [ ] Document architecture (link to specs)
- [ ] List controls (desktop + Android)

**Acceptance Criteria:**
- Someone can build and run from README alone
- Preprocessor usage clear
- Android setup documented

---

### Task 27: Cleanup + final verification
**Priority:** High  
**Estimated Time:** 2 hours

**Subtasks:**
- [ ] Remove dead code, unused includes
- [ ] Run clang-format on all source files
- [ ] Verify -Wall -Wextra -Wpedantic with zero warnings
- [ ] Verify clean build from scratch (rm -rf build && cmake && make)
- [ ] Verify Android clean build
- [ ] Final test run (all tests pass)
- [ ] Final ASan run (clean)
- [ ] Final device test (Xiaomi Pad 6)

**Acceptance Criteria:**
- Zero compiler warnings
- Clean build works
- All tests pass
- ASan clean
- App runs on device

---

### Task 28: Release preparation
**Priority:** Low  
**Estimated Time:** 1 hour

**Subtasks:**
- [ ] Tag release version
- [ ] Generate release APK
- [ ] Verify APK installs on clean device
- [ ] Write release notes
- [ ] Update specs with any deviations from plan

**Acceptance Criteria:**
- Release APK builds and installs
- Release notes document what was built
- Specs match implementation

---

## Task Dependencies

```
Task 1 (CMake) → All other tasks
Task 2 (Platform interface) → Task 12 (Desktop app), Task 19 (Android platform)
Task 3 (Tile ID + types) → Task 7 (Loader), Task 8 (Geometry), Task 15 (Cache)
Task 4 (Color table) → Task 11 (Draw loop)
Task 5 (Proto schema) → Task 6 (Preprocessor), Task 7 (Loader)
Task 6 (Preprocessor) → Task 21 (Tile deployment), Task 25 (India benchmark)
Task 7 (Tile loader) → Task 16 (Background thread), Task 17 (Engine)
Task 8 (Geometry builder) → Task 10 (Tile upload), Task 11 (Draw loop)
Task 9 (Shader) → Task 10 (Tile upload)
Task 10 (Tile upload) → Task 11 (Draw loop)
Task 11 (Draw loop) → Task 12 (Desktop app)
Task 12 (Desktop app) → Task 17 (Engine integration test)
Task 13 (Camera) → Task 14 (Tile zoom), Task 17 (Engine)
Task 14 (Tile zoom + visible tiles) → Task 17 (Engine)
Task 15 (Tile cache) → Task 16 (Background thread), Task 17 (Engine)
Task 16 (Background thread) → Task 17 (Engine)
Task 17 (Engine) → Task 22 (Android render test), Task 23 (Test suite), Task 25 (India benchmark)
Task 18 (Android project) → Task 19 (Android platform)
Task 19 (Android platform) → Task 20 (Touch input), Task 22 (Android render test)
Task 20 (Touch input) → Task 22 (Android render test)
Task 21 (Tile deployment) → Task 22 (Android render test)
Task 22 (Android render) → Task 25 (India benchmark)
Task 23 (Test suite) → Task 27 (Cleanup)
Task 24 (Profiling) → Task 25 (India benchmark)
Task 25 (India benchmark) → Task 27 (Cleanup)
Task 26 (Documentation) → Task 28 (Release)
Task 27 (Cleanup) → Task 28 (Release)
```

---

## Definition of Done

A task is complete when:
- [ ] Code implemented
- [ ] Unit tests written and passing (where applicable)
- [ ] Manual testing completed
- [ ] No compiler warnings
- [ ] Code follows style guidelines (clang-format)
- [ ] Committed to repository

---

## Notes

- Each task should be completable in 1-5 hours
- If a task takes longer, break it down further
- Test after each task
- Commit after each task
- Don't move to next task until current is working
- Update this TASKS.md file if estimates or dependencies change
- Android tasks (18-22) require a physical device for final testing
- India preprocessing (Task 25) may take hours — run overnight

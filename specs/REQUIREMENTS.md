# Requirements Specification
## Interactive 2D Map Renderer

**Version:** 2.0  
**Date:** July 3, 2026  
**Status:** Specification — Redesigned for 2D tiled rendering on Android

---

## 1. Functional Requirements

### 1.1 Data Pipeline

**FR-1.1: OSM Tile Preprocessing**
- **Priority:** Critical
- **Description:** A Python preprocessor shall read OSM PBF files and produce a tile pyramid in protobuf format
- **Input:** OSM PBF file (e.g., NewDelhi.osm.pbf, india-260623.osm.pbf)
- **Output:** Directory of z/x/y tile files in `data/tiles/<region>/`
- **Acceptance Criteria:**
  - Converts WGS84 lat/lon to per-tile local ENU meters
  - Splits features into standard slippy-map tiles (z/x/y)
  - Applies Douglas-Peucker simplification per zoom level
  - Includes building heights for future 3D support (unused in 2D rendering)
  - Compresses tile files with zstd
  - Skips empty tiles (no output file for tiles with zero features)
  - Generates a metadata file with dataset bounds and reference origin

**FR-1.2: Tile Loading**
- **Priority:** Critical
- **Description:** The engine shall load tiles from storage on demand based on the current viewport
- **Input:** Tile coordinates (z, x, y) and tile directory path
- **Output:** Deserialized tile data in memory
- **Acceptance Criteria:**
  - Loads only tiles visible in the current camera viewport
  - Decompresses zstd on load
  - Deserializes protobuf into internal structures
  - Reserves vectors from tile header counts (no reallocation during parse)
  - Handles missing or corrupt tile files gracefully (skip, log warning)

**FR-1.3: Tile Caching**
- **Priority:** Critical
- **Description:** The engine shall maintain an LRU cache of loaded tiles
- **Acceptance Criteria:**
  - Configurable maximum cache size (default: 64 tiles)
  - Evicts least-recently-used tiles when cache is full
  - Frees GPU buffers on eviction
  - Thread-safe (loading thread writes, render thread reads)

**FR-1.4: Background Tile Loading**
- **Priority:** High
- **Description:** The engine shall load tiles in a background thread to prevent frame stalls
- **Acceptance Criteria:**
  - Render thread never blocks on I/O
  - Worker thread loads requested tiles from storage
  - Newly loaded tiles appear when ready (no explicit frame-skip required)
  - Prefetch ring: loads tiles one ring beyond visible viewport
  - Cancel pending loads for tiles that scroll out of view

### 1.2 Rendering

**FR-2.1: 2D Map Rendering**
- **Priority:** Critical
- **Description:** The engine shall render a 2D top-down map view
- **Output:** Rendered framebuffer
- **Acceptance Criteria:**
  - Renders ground (solid color, no grid for v2.0)
  - Renders polygon fills (parks, water, landuse) with correct colors
  - Renders roads as quads with correct widths
  - Renders building footprints as filled polygons
  - Correct draw order: ground → water → parks → landuse → roads → buildings
  - No depth buffer (draw order handles occlusion)
  - VSync enabled (SDL_GL_SetSwapInterval(1))

**FR-2.2: Tile Stitching**
- **Priority:** Critical
- **Description:** The engine shall render multiple tiles seamlessly in a single viewport
- **Acceptance Criteria:**
  - No visible seams between adjacent tiles
  - Per-tile origin offset applied via uniform in vertex shader
  - Tiles at the same zoom level render at consistent scale
  - Camera position independent of tile boundaries

**FR-2.3: Color Styling**
- **Priority:** Medium
- **Description:** The engine shall apply colors to features based on a hardcoded style table
- **Acceptance Criteria:**
  - Colors defined in a C++ header (no JSON parsing for v2.0)
  - Colors for: building, road_primary, road_secondary, water, park, landuse, ground
  - Colors passable as uniform to the single shader program
  - Future hook: style table structure allows JSON loading in a later version

### 1.3 Camera System

**FR-3.1: 2D Orthographic Camera**
- **Priority:** Critical
- **Description:** The engine shall provide an orthographic 2D camera with pan and zoom
- **Input:** Look-at point (x, z in world meters), zoom level
- **Output:** Projection and view matrices
- **Acceptance Criteria:**
  - Pan in world meters (drag and arrow keys)
  - Zoom by scroll wheel and +/- keys
  - Zoom clamped to valid range for the current tile zoom level
  - Aspect ratio preserved
  - Dirty flag: matrices recomputed only when camera state changes
  - Determines visible tile range from current viewport

**FR-3.2: Tile-Aware Zoom**
- **Priority:** High
- **Description:** The camera shall select the appropriate tile zoom level based on the current view scale
- **Acceptance Criteria:**
  - Maps camera zoom to the nearest tile zoom level (8, 12, 15, or 17)
  - Switches tile data source when crossing zoom thresholds
  - Brief transition acceptable (lower LOD visible while higher LOD loads)

### 1.4 Input Handling

**FR-4.1: Touch Input (Android)**
- **Priority:** Critical
- **Description:** The engine shall respond to touch input on Android
- **Controls:**
  - Single-finger drag: Pan
  - Two-finger pinch: Zoom
- **Acceptance Criteria:**
  - Smooth panning, no jitter
  - Pinch zoom proportional to finger distance change
  - Touch events passed from Android app to engine via platform interface

**FR-4.2: Mouse/Keyboard Input (Desktop)**
- **Priority:** High
- **Description:** The engine shall respond to mouse and keyboard input on desktop
- **Controls:**
  - ESC: Quit
  - Mouse drag: Pan
  - Scroll wheel: Zoom
  - +/-: Zoom
  - Arrow keys: Pan
- **Acceptance Criteria:**
  - All controls responsive
  - Pan speed proportional to zoom level

**FR-4.3: Window Events**
- **Priority:** High
- **Description:** The engine shall handle window resize and close events
- **Acceptance Criteria:**
  - Viewport updates on resize
  - Clean shutdown on close

### 1.5 Platform Support

**FR-5.1: Desktop Application (Linux)**
- **Priority:** Critical
- **Description:** A desktop application shall run the engine on Linux using SDL2
- **Acceptance Criteria:**
  - Creates SDL2 window with OpenGL 3.3 Core context
  - Passes input events to engine
  - Runs the engine render loop
  - Uses the same tile pipeline as the Android app

**FR-5.2: Android Application**
- **Priority:** Critical
- **Description:** An Android application shall run the engine on Android using NativeActivity + EGL
- **Acceptance Criteria:**
  - Separate Android Studio project in `android_app/` directory
  - Loads engine as a native library via JNI
  - Creates EGL surface with GLES 3.0 context
  - Passes touch input to engine via platform interface
  - Reads tile data from app-internal or external storage
  - Targets Android 13 (API 33), minSdk 24 (Android 7.0)
  - Runs on Xiaomi Pad 6

**FR-5.3: Platform Abstraction**
- **Priority:** Critical
- **Description:** The engine core shall contain no platform-specific code
- **Acceptance Criteria:**
  - No SDL2, Android, or Linux headers in engine core
  - Platform interface defines: GL function loader, surface creation, input injection, file access
  - Desktop app and Android app each implement the platform interface
  - Engine tests run without a window (headless mock platform)

### 1.6 Performance

**FR-6.1: Frame Rate**
- **Priority:** Critical
- **Description:** The engine shall maintain smooth frame rates on the target device
- **Conditions:**
  - Xiaomi Pad 6, New Delhi tile dataset, viewport at zoom 15-17
  - India tile dataset, viewport at zoom 8-15
- **Acceptance Criteria:**
  - 60 FPS target on Xiaomi Pad 6 with New Delhi data
  - 40 FPS minimum on Xiaomi Pad 6 with India data at high zoom
  - No frame stalls > 16 ms during tile loading (background thread)

**FR-6.2: Memory Usage**
- **Priority:** Critical
- **Description:** The engine shall stay within memory budgets on the target device
- **Acceptance Criteria:**
  - CPU RAM: < 150 MB (tile cache + engine overhead)
  - GPU memory: < 50 MB (vertex buffers, uniforms)
  - No memory leaks (AddressSanitizer on desktop, leak detection on Android)
  - Tile cache eviction prevents unbounded growth

**FR-6.3: Compiled Data Size**
- **Priority:** High
- **Description:** The compiled tile data shall be compact for device storage
- **Acceptance Criteria:**
  - New Delhi tiles: < 20 MB total (all zoom levels, zstd-compressed)
  - Tiles compressed with zstd
  - Empty tiles not generated (no wasted storage)
  - Only needed tiles transferred to device

**FR-6.4: Startup Time**
- **Priority:** Medium
- **Description:** The engine shall start quickly
- **Acceptance Criteria:**
  - Desktop: < 1 second to first frame
  - Android: < 2 seconds to first frame
  - No blocking I/O on startup (tiles load in background)

---

## 2. Non-Functional Requirements

### 2.1 Code Quality

**NFR-1.1: Coding Standards**
- **Priority:** High
- **Description:** Code shall follow C++ Core Guidelines
- **Acceptance Criteria:**
  - No compiler warnings with -Wall -Wextra -Wpedantic
  - Consistent formatting (clang-format)
  - Meaningful variable names
  - Functions < 50 lines
  - `DEBUG_LOG` macro available when `MAP_RENDERER_DEBUG` is defined
  - `GL_CHECK` macro available when `MAP_RENDERER_DEBUG` is defined

**NFR-1.2: Documentation**
- **Priority:** Medium
- **Description:** Public APIs shall be documented
- **Acceptance Criteria:**
  - Doxygen comments for engine public headers
  - README with build instructions for both desktop and Android
  - Preprocessor usage documented

**NFR-1.3: Testing**
- **Priority:** High
- **Description:** The engine shall have comprehensive tests
- **Acceptance Criteria:**
  - Unit tests for: camera, tile loader, geometry builder, tile cache, color table
  - Integration test: load tile → build geometry → verify vertex data
  - All tests pass
  - Memory correctness checked with AddressSanitizer on desktop
  - Engine tests run headless (no GL context required for non-rendering tests)

**NFR-1.4: Debug Logging**
- **Priority:** High
- **Description:** Source files shall include `DEBUG_LOG` and `GL_CHECK` macros
- **Acceptance Criteria:**
  - `DEBUG_LOG(...)` expands to formatted log in debug builds, nothing in release
  - `GL_CHECK` calls `glGetError()` and logs in debug builds, nothing in release
  - Zero runtime overhead when disabled

### 2.2 Portability

**NFR-2.1: Cross-Platform**
- **Priority:** Critical
- **Description:** Engine core shall compile on both Linux (desktop) and Android NDK
- **Acceptance Criteria:**
  - No platform-specific code in engine core
  - Platform abstraction interface isolates SDL2/EGL/Android APIs
  - Desktop app uses SDL2 + GL 3.3 Core
  - Android app uses NativeActivity + EGL + GLES 3.0
  - Shaders compile under both GL 3.3 Core and GLES 3.0

**NFR-2.2: Compiler Support**
- **Priority:** High
- **Description:** Code shall compile with C++17
- **Acceptance Criteria:**
  - GCC 12+ (desktop)
  - Clang 15+ (desktop)
  - Android NDK r25+ (Clang, C++17)

### 2.3 Maintainability

**NFR-3.1: Modular Design**
- **Priority:** High
- **Description:** Code shall be organized in clear modules
- **Acceptance Criteria:**
  - Engine library separate from apps
  - Clear separation: core (camera, input, platform) / data (tiles, geometry) / render (GL, shaders)
  - Minimal coupling between modules
  - 3D rendering hooks present but unimplemented (CameraMode::MODE_3D reserved)

**NFR-3.2: Build System**
- **Priority:** High
- **Description:** Build system shall be simple and reproducible
- **Acceptance Criteria:**
  - CMake 3.20+ for engine and desktop app
  - Gradle for Android app
  - Out-of-source builds
  - protobuf codegen integrated into CMake
  - GLAD generated or fetched via CMake

### 2.4 Reliability

**NFR-4.1: Error Handling**
- **Priority:** High
- **Description:** The engine shall handle errors gracefully
- **Acceptance Criteria:**
  - No crashes on missing tiles (skip and continue)
  - No crashes on corrupt tile data (skip and log)
  - No crashes on GL errors (log and continue in debug, assert in debug)
  - Clean shutdown on all platforms

**NFR-4.2: Resource Management**
- **Priority:** High
- **Description:** The engine shall properly manage resources
- **Acceptance Criteria:**
  - RAII for all GL resources (VBOs, VAOs, shader programs)
  - Tile cache evicts and frees GPU buffers on eviction
  - No memory leaks (ASan/LSan clean on desktop)
  - All GL resources deleted on shutdown

---

## 3. Constraints

### 3.1 Technical Constraints

**TC-1:** OpenGL ES 3.0 target (develop on desktop GL 3.3 Core, shaders ES-compatible)
**TC-2:** C++17 (no C++20/23 features)
**TC-3:** SDL2 for desktop window/input
**TC-4:** NativeActivity + EGL for Android
**TC-5:** GLM for math
**TC-6:** CMake 3.20+ for engine and desktop app
**TC-7:** Gradle for Android app
**TC-8:** protobuf for tile serialization
**TC-9:** zstd for tile compression
**TC-10:** GLAD for GL function loading
**TC-11:** Google Test for unit tests
**TC-12:** Slippy-map tiling (z/x/y, Web Mercator addressing)
**TC-13:** Per-tile local ENU meters for geometry storage
**TC-14:** No nlohmann/json in v2.0 (hardcoded color table)
**TC-15:** No depth buffer in v2.0 (draw order handles occlusion)
**TC-16:** No 3D rendering in v2.0 (architecture hooks only)

### 3.2 Resource Constraints

**RC-1:** GPU memory: < 50 MB
**RC-2:** CPU RAM: < 150 MB
**RC-3:** Compiled tiles (New Delhi): < 20 MB
**RC-4:** Frame rate: 60 FPS target, 40 FPS minimum
**RC-5:** Frame stall: < 16 ms during tile loading
**RC-6:** Startup: < 1s desktop, < 2s Android
**RC-7:** No blocking I/O on the render thread
**RC-8:** No per-frame heap allocations in the render loop

### 3.3 Platform Constraints

**PC-1:** Target device: Xiaomi Pad 6 (Android 13, API 33)
**PC-2:** Min SDK: Android 7.0 (API 24)
**PC-3:** GLES 3.0 required (Adreno 650 supports GLES 3.2)
**PC-4:** Android NDK r25+
**PC-5:** Limited device storage — minimize tile data size

---

## 4. Acceptance Criteria

### 4.1 Definition of Done

A feature is "done" when:
- [ ] Code implemented
- [ ] Unit tests written and passing
- [ ] No compiler warnings
- [ ] Manual testing completed
- [ ] Self-reviewed and tested
- [ ] Documentation updated
- [ ] Committed to repository

### 4.2 Release Criteria

v2.0 is ready for release when:
- [ ] Desktop app renders New Delhi tiles smoothly (60 FPS)
- [ ] Android app renders New Delhi tiles on Xiaomi Pad 6 (60 FPS)
- [ ] Android app renders India tiles at various zoom levels (40+ FPS)
- [ ] Memory budget met on Xiaomi Pad 6 (< 150 MB RAM, < 50 MB GPU)
- [ ] All unit and integration tests pass
- [ ] No memory leaks (ASan clean on desktop)
- [ ] No GL errors in debug builds
- [ ] Build reproducible on clean system (both desktop and Android)

---

## 5. Priority Matrix

| Priority | Requirements |
|----------|-------------|
| **Critical** | FR-1.1, FR-1.2, FR-1.3, FR-2.1, FR-2.2, FR-3.1, FR-4.1, FR-5.1, FR-5.2, FR-5.3, FR-6.1, FR-6.2, NFR-2.1 |
| **High** | FR-1.4, FR-2.3, FR-3.2, FR-4.2, FR-4.3, FR-6.3, NFR-1.1, NFR-1.3, NFR-1.4, NFR-2.2, NFR-3.1, NFR-3.2, NFR-4.1, NFR-4.2 |
| **Medium** | FR-6.4, NFR-1.2 |

---

## 6. Out of Scope (v2.0)

- 3D rendering (building extrusion, lighting, perspective camera)
- Depth buffer
- Vulkan
- nlohmann/json style loading (hardcoded colors for v2.0)
- Polygon holes (courtyards within buildings)
- Terrain elevation
- Shadows
- Routing/navigation
- Real-time data updates
- Server-side tile serving
- Tile streaming over network (local storage only)
- Custom indexed file format (directory of tiles for v2.0, indexed file later)
- Windows and macOS desktop support
- iOS support

These are deferred to a future version. The architecture retains hooks for 3D and JSON styles.

---

## 7. Success Metrics

The project is successful if:
1. **Performance:** 60 FPS on Xiaomi Pad 6 with New Delhi data
2. **Scale:** Renders India dataset without OOM or frame death
3. **Memory:** < 150 MB RAM, < 50 MB GPU on target device
4. **Storage:** New Delhi tiles < 20 MB on device
5. **Quality:** All tests pass, ASan clean, no GL errors
6. **Portability:** Same engine runs on Linux desktop and Android tablet

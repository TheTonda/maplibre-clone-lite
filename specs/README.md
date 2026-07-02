# Specifications

This directory contains the complete specification for the Interactive 2D Map Renderer project.

## Documents

### 1. [HLD.md](HLD.md) - High-Level Design
System architecture, tiling model, component overview, technology stack, and development phases.

### 2. [LLD.md](LLD.md) - Low-Level Design
Detailed module specifications, interfaces, data structures, tile schema, shaders, and implementation details.

### 3. [REQUIREMENTS.md](REQUIREMENTS.md) - Requirements
Functional and non-functional requirements, constraints, acceptance criteria, and resource budgets.

### 4. [TASKS.md](TASKS.md) - Task Breakdown
Detailed task and subtask breakdown with time estimates, dependencies, and Definition of Done.

## Version History

| Version | Date | Summary |
|---------|------|---------|
| 1.0-1.2 | July 2, 2026 | Original Vulkan 3D renderer spec (archived on try_2 branch) |
| 2.0 | July 3, 2026 | Complete redesign: 2D-only, OpenGL ES 3.0, slippy-map tiling, Android target |

## v2.0 Design Rationale

The original v1.x specification targeted a Vulkan 3D renderer for desktop Linux. During implementation review, the scope shifted to a 2D tiled map renderer targeting budget Android devices. This required a fundamental redesign:

1. **3D dropped, architecture kept** — Building extrusion, lighting, depth buffer, and 3D camera removed. Module structure and data schema retain 3D hooks for a future version.
2. **Vulkan → OpenGL ES 3.0** — Vulkan's 2000+ lines of boilerplate (instance, device, swapchain, render pass, framebuffers, sync objects) eliminated. GLES 3.0 runs on the target device and virtually all Android phones from the last decade.
3. **Monolithic load → slippy-map tiling** — The original "load all data into memory" model cannot work for India (1.6 GB PBF, tens of millions of features). Standard z/x/y tile pyramid with LRU cache and background loading.
4. **C++23 → C++17** — Broader compiler support on Android NDK, fewer compatibility issues.
5. **Single binary → engine library + thin apps** — Core rendering logic is a platform-independent library. Desktop app (SDL2) and Android app (JNI) are thin wrappers.

## Key Decisions

- **OpenGL ES 3.0** target (develop on desktop GL 3.3 Core, ES-compatible shaders)
- **Slippy-map tiling** (z/x/y, Web Mercator addressing, 4 initial zoom levels: 8/12/15/17)
- **Per-tile local ENU meters** for geometry storage (float32, physically accurate, future-3D ready)
- **zstd compression** for tile files (minimizes device storage)
- **Protobuf** for tile serialization (compact, schema evolution, C++ and Python codegen)
- **GLM** for math (header-only, trivial, needed for future 3D)
- **SDL2** for desktop window/input (Android uses NativeActivity + EGL)
- **Engine as static/shared library** with platform abstraction interface
- **Hardcoded color table** in a header (no nlohmann/json dependency for v2.0)
- **Single shader program** for all 2D geometry (color via uniform)
- **VSync enabled** (60 FPS target, driver blocks on swap, CPU sleeps between frames)
- **DEBUG_LOG** macro gated by `MAP_RENDERER_DEBUG`
- **GL_CHECK** macro for error detection in debug builds
- **Shaders embedded as C++ string constants** (no runtime file I/O)

## Target Device

**Xiaomi Pad 6** (Android 13)
- SoC: Snapdragon 870 (8-core: 1×3.2 GHz A77 + 3×2.4 GHz A77 + 4×1.8 GHz A55)
- GPU: Adreno 650
- RAM: 6 GB (approx. 1-2 GB available to app)
- Storage: 128 GB (user reports limited free space)
- Display: 11" 2880×1800, 144 Hz

## Test and Benchmark Data

| Dataset | Size | Role |
|---------|------|------|
| NewDelhi.osm.pbf | 35 MB | Development and testing (both desktop and Android) |
| india-260623.osm.pbf | 1.6 GB | End-to-end performance benchmarking |

Both files reside in `~/Downloads/`. Preprocessing output goes to `data/` (gitignored).

## Specification Principles

1. **Clear and Unambiguous** - No room for misinterpretation
2. **Testable** - Every requirement can be verified
3. **Traceable** - Requirements link to tasks
4. **Maintainable** - Easy to update as project evolves
5. **Complete** - Covers all aspects of the system

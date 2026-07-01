# High-Level Design (HLD) Specification
## Interactive 3D Map Renderer

**Version:** 1.1  
**Date:** July 2, 2026  
**Status:** Specification - Optimized after review

---

## 1. Executive Summary

This document describes the high-level architecture of an interactive 3D map renderer for OpenStreetMap data. The renderer will support both 2D top-down viewing and 3D perspective viewing with extruded buildings, similar to MapLibre Native but built from scratch using Vulkan and modern C++.

### 1.1 Goals
- Render OpenStreetMap (OSM) data including buildings, roads, parks, and water
- Support interactive 2D and 3D camera with pan, zoom, and tilt
- Achieve 30+ FPS with 40,000+ buildings on modern GPU
- Clean, maintainable codebase with comprehensive tests
- Linux primary target; architect for future Android support

### 1.2 Non-Goals
- Real-time data updates
- Server-side tile rendering
- Advanced lighting (shadows, ambient occlusion)
- Building interior rendering
- Routing/navigation
- Offline tile caching
- Windows and macOS v1.0 support

---

## 2. System Architecture

### 2.1 Component Overview

```
┌─────────────────────────────────────────────────────────────┐
│                      Application Layer                       │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                  │
│  │ Window   │  │ Input    │  │ Camera   │                  │
│  │ Manager  │  │ State    │  │ System   │                  │
│  └──────────┘  └──────────┘  └──────────┘                  │
└────────────────────┬────────────────────────────────────────┘
                      │
┌────────────────────┴────────────────────────────────────────┐
│                      Renderer Layer                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  │
│  │ 2D Path  │  │ 3D Path  │  │ Buildings│  │ Features │  │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘  │
└────────────────────┬────────────────────────────────────────┘
                      │
┌────────────────────┴────────────────────────────────────────┐
│                      Data Layer                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                  │
│  │ OSM      │  │ Geometry │  │ Style    │                  │
│  │ Loader   │  │ Builder  │  │ Engine   │                  │
│  └──────────┘  └──────────┘  └──────────┘                  │
└────────────────────┬────────────────────────────────────────┘
                      │
┌────────────────────┴────────────────────────────────────────┐
│                    Graphics Layer                            │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  │
│  │ Vulkan   │  │ Shaders  │  │ Pipelines│  │ Buffers  │  │
│  │ Context  │  │ (GLSL)   │  │          │  │          │  │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Layer Responsibilities

#### Application Layer
- SDL window management
- Event handling (keyboard, mouse)
- Input state tracking
- Camera system (state, matrices, constraints)
- Main render loop
- FPS counter and statistics

#### Renderer Layer
- Orchestrates rendering passes
- Manages render state
- Coordinates between data and graphics layers
- Implements 2D and 3D rendering paths

#### Data Layer
- Loads OSM data from protobuf
- Builds 3D geometry from 2D footprints
- Parses style definitions
- Applies building height fallback rules

#### Graphics Layer
- Vulkan API abstraction
- Shader compilation and management
- Pipeline creation and caching
- GPU buffer management

### 2.3 Data Flow

```
OSM PBF File
    ↓
Python Preprocessor (extract_geometry.py)
    ↓
Protobuf File (osm_data.bin)
    ↓
OSM Loader (C++) using protobuf C++ runtime
    ↓
Geometry Builder (C++)
    ↓
GPU Buffers (Vulkan)
    ↓
Shaders (GLSL)
    ↓
Framebuffer → Display
```

---

## 3. Technology Stack

### 3.1 Core Technologies
- **Language:** C++23
- **Graphics API:** Vulkan 1.2
- **Window System:** SDL2
- **Math Library:** GLM
- **Build System:** CMake 3.20+
- **Test Framework:** Google Test (C++), unittest (Python)
- **Serialization:** protobuf (preprocessed OSM data)
- **Configuration/Style:** nlohmann/json (header-only, style definitions)

### 3.2 External Dependencies
- Vulkan SDK
- SDL2 development libraries
- GLM headers
- Google Test
- protobuf C++ runtime and protoc
- nlohmann/json (header-only)
- Python 3.8+ (for OSM preprocessing)
- Python protobuf package and osmium (or equivalent)

### 3.3 Supported Platforms
- Linux (primary target for v1.0)
- Android (architect for future; use ANativeWindow/SDL Android support)
- Windows and macOS explicitly out of scope for v1.0

---

## 4. Key Subsystems

### 4.1 Coordinate System

The renderer uses a single, consistent coordinate space so that 2D and 3D modes share the same data and camera math.

- **Input:** WGS84 lat/lon (degrees) from OSM
- **Preprocessing:** Convert WGS84 to a local East-North-Up (ENU) Cartesian frame centered on the dataset center
- **Storage:** Local ENU meters relative to data center (`x` east, `y` up, `z` north)
- **Rendering:** Same local ENU meters; no further runtime reprojection

**Why ENU meters?**
- Building heights and road widths are expressed in real-world meters. Keeping x/z in meters means extruded buildings have physically correct proportions anywhere on Earth, without latitude-dependent scale distortion.
- The camera can use intuitive units: zoom “show 200 m”, distance “500 m from look-at”, tilt in degrees.
- 2D orthographic and 3D perspective share the same world-space origin and axes.

**Conversion formula (Python preprocessor):**
```python
lat0, lon0 = dataset_center_latitude, dataset_center_longitude
R = 6371000.0  # Earth radius in meters
x = R * cos(radians(lat0)) * radians(lon - lon0)
z = R * radians(lat - lat0)
```
The loader receives `(x, z)` in meters and stores them directly. `y` is generated by the geometry builder (0 for ground, height for building tops).

### 4.2 Camera System
- **2D Mode:** Orthographic projection, pan/zoom
- **3D Mode:** Perspective projection with tilt
- **Parameters:** position (x, y), distance, tilt, rotation
- **Controls:** Arrow keys, mouse drag, Q/E (tilt), A/D (rotate)

### 4.3 Rendering Pipelines

#### 2D Pipeline
- Orthographic projection
- Render order: ground → fills → lines
- No depth testing
- Simple shaders

#### 3D Pipeline
- Perspective projection
- Render order: ground → features → buildings
- Depth testing enabled
- Per-vertex lighting
- Distance-based culling

### 4.4 Data Structures

#### Building
- Footprint (polygon in local ENU meters)
- Height (meters)
- Height source (`tag`, `levels`, or `default`)
- Optional: color, type

**Height fallback policy:**
1. Use `height` tag when present and parseable.
2. Else use `building:levels` tag × default floor height (3.0 m).
3. Else use a configurable default height (9.0 m for generic buildings).

#### Road
- Line string (in local ENU meters)
- Optional: width, type
- Default width: 6.0 m if no OSM width tag

#### Feature (Park/Water/Landuse)
- Polygon
- Color (from style)

---

## 5. Performance Requirements

### 5.1 Target Performance
- 30+ FPS on GTX 1060 / RX 580 or equivalent
- 60 FPS on RTX 3060 / RX 6700 XT or equivalent
- Support 40,000+ buildings
- Support 100,000+ road segments
- Initial load time < 5 seconds

### 5.2 Memory Budget
- GPU memory: < 500 MB
- System memory: < 200 MB
- Streaming not required (load all data upfront)

### 5.3 Optimization Strategies
- Frustum culling (skip off-screen objects)
- Distance-based LOD (future)
- Batch rendering by material
- Minimize draw calls
- Render roads as quads instead of relying on Vulkan line width

---

## 6. Testing Strategy

### 6.1 Test Levels
- **Unit Tests:** Test individual functions
- **Integration Tests:** Test component interaction
- **Visual Tests:** Manual verification of rendering
- **Performance Tests:** FPS and memory benchmarks

### 6.2 Test Coverage Goals
- 80%+ code coverage
- All public APIs tested
- Edge cases covered
- Error paths tested

### 6.3 Continuous Integration
- All tests must pass before commit
- Build must succeed on Linux
- No memory leaks detected by AddressSanitizer / LeakSanitizer
- Valgrind as a best-effort additional check (Vulkan driver allocations may produce false positives)

---

## 7. Development Phases

### Phase 1: Foundation (Week 1)
- Project setup
- Build system
- Basic Vulkan window
- Hello triangle

### Phase 2: Data Pipeline (Week 2)
- OSM JSON loader
- Coordinate conversion
- Python preprocessing tool

### Phase 3: 2D Rendering (Week 3)
- Orthographic camera
- 2D shaders
- Render polygons and lines

### Phase 4: 3D Rendering (Week 4)
- Perspective camera
- Building extrusion
- 3D shaders with lighting

### Phase 5: Interactivity (Week 5)
- Mouse/keyboard input
- Camera controls
- Pan/zoom/tilt

### Phase 6: Polish (Week 6)
- Performance optimization
- Visual polish
- Documentation

---

## 8. Risks and Mitigations

### 8.1 Technical Risks
- **Risk:** Vulkan complexity  
  **Mitigation:** Start with simple examples, add complexity gradually

- **Risk:** Coordinate system bugs  
  **Mitigation:** Comprehensive tests for coordinate conversions

- **Risk:** Performance issues  
  **Mitigation:** Profile early and often, implement culling

### 8.2 Project Risks
- **Risk:** Scope creep  
  **Mitigation:** Stick to defined requirements, defer features

- **Risk:** Over-engineering  
  **Mitigation:** Implement simplest solution first, refactor later

---

## 9. Success Criteria

The project is successful when:
- [ ] Renders OSM data correctly in 2D mode
- [ ] Renders OSM data correctly in 3D mode with extruded buildings
- [ ] Interactive camera controls work smoothly
- [ ] Achieves 30+ FPS with full dataset
- [ ] All tests pass (unit + integration)
- [ ] Code is well-documented and maintainable
- [ ] Build is reproducible and portable

---

## 10. Open Questions

1. Should we support multiple style layers with z-ordering? *(Answer: not in v1.0)*
2. Do we need support for custom shaders? *(Answer: not in v1.0, but design for it)*
3. Should we implement building shadows? *(Answer: not in v1.0)*
4. Do we need support for terrain elevation? *(Answer: not in v1.0)*
5. Should we support MVT (Mapbox Vector Tiles) format? *(Answer: not in v1.0)*

These questions will be addressed in the Low-Level Design document.

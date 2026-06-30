# Map Renderer v2 — Architecture Deep-Dive

## System Overview

The map renderer is a complete GPU-accelerated vector tile renderer built from scratch. It loads Mapbox Vector Tiles (MVT), parses a JSON style sheet, and renders the result via Vulkan to an SDL2 window.

```
MVT Vector Tile (PBF) ──→ TileDecoder ──→ Geometry Buffers ──→ Vulkan Pipeline ──→ Framebuffer
JSON Style Sheet ──────→ StyleParser  ──→ Uniforms/Bindings ─┘
Camera (pan/zoom) ─────→ Projection   ──→ NDC coordinates  ──┘
```

## Component Breakdown

### 1. MVT Parser (`src/mvt_parser.h`)

The MVT parser reads Mapbox Vector Tile files encoded in Protocol Buffers (PBF) format. It is header-only and depends only on protobuf-lite.

**Key design choices:**
- No protobuf-generated code — manually walks the wire format using `CodedInputStream`
- Supports all 7 protobuf value types (string, float, double, int, uint, sint, bool)
- Handles zigzag-decoded variable-length integers for geometry commands
- Geometry commands use the MVT spec encoding: MoveTo=1, LineTo=2, ClosePath=7

**Parsing pipeline:**
1. Read raw bytes into `CodedInputStream`
2. Parse top-level `Tile` message: iterate submessages (layers)
3. For each layer: read name (field 1), features (field 3), keys (field 4), values (field 5), extent (field 7), version (field 2)
4. For each feature: read id (field 1), tags (field 2, packed), geom_type (field 3), geometry (field 4, packed)
5. Tags are stored as key_id → value mapping using the layer's keys/values tables

**Coordinate system:** Tile-local coordinates are in the range `[0, extent]` (default 4096). These must be normalized to `[-1, 1]` clip space before GPU upload.

### 2. Render Data (`src/render_data.h`)

Converts parsed MVT geometry into vertex/index arrays suitable for Vulkan.

**Line rendering:**
- LINESTRING features are decoded into `LineSegment` structs with tile-local coordinates
- Multiple line segments from the same layer are batched into a `LineBatch`
- `0xFFFFFFFF` is used as a primitive restart marker between separate features
- Coordinates scaled from tile space to clip space at extraction time

**Polygon rendering:**
- POLYGON features contain one or more rings (outer ring + holes)
- Only the outer ring is used for rendering (holes are ignored in this implementation)
- Rings are fan-triangulated into triangle lists for `VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST`
- Fan triangulation: connect each consecutive pair of vertices to the first vertex

**Coordinate normalization:**
```
clip_x = (tile_x / extent) * 2.0 - 1.0
clip_y = 1.0 - (tile_y / extent) * 2.0   // flip Y because tile coords are top-down
```

### 3. Style Engine (`src/style_engine.h`)

A minimal JSON parser with no external dependencies. Parses a subset of the MapLibre/Mapbox GL Style Spec.

**Supported style elements:**
- `background` layer type with `paint.background-color`
- `fill` layer type with `paint.fill-color` and `paint.fill-opacity`
- `line` layer type with `paint.line-color`, `paint.line-width`, `paint.line-opacity`

**Matching algorithm:**
1. Iterate all loaded style layers
2. Compare `layer.id` against the requested layer name
3. Also match against the wildcard `*` for fallback rules
4. Return the first matching layer's paint properties as a `StyleRule`

**Color representation:** Stored as `glm::vec3` (normalized 0.0-1.0 RGB). Hex strings are parsed manually with `parse_hex_color()`.

### 4. Vulkan Renderer (`src/main.cpp`)

The main application file contains the complete Vulkan rendering pipeline (~1300 lines).

**Initialization order:**
1. **SDL2 window** (1024x768) — creates the OS window
2. **Vulkan instance** — targets API 1.3, enables `VK_EXT_debug_utils`
3. **Swapchain surface** — created from the SDL2 window
4. **Queue families** — finds graphics and present queue families
5. **Logical device** — creates with swapchain extension
6. **Swapchain** — queries capabilities, creates with chosen format
7. **Image views** — one per swapchain image
8. **Render pass** — single color attachment, load action = clear
9. **Camera UBO** — 64-byte uniform buffer for projection matrix
10. **Descriptor set** — binding 0, uniform buffer descriptor

**Three graphics pipelines are created:**

| Pipeline | Shader Pair | Topology | Purpose |
|----------|------------|----------|---------|
| Triangle demo | triangle.vert/frag | TRIANGLE_LIST | Hardcoded triangle, no vertex buffers |
| Lines | line.vert/frag | LINE_STRIP (primitive restart) | Road/street geometries |
| Fills | fill.vert/frag | TRIANGLE_LIST | Building/water/land polygons |

**Camera UBO layout:**
```glsl
layout(binding = 0) uniform CameraUBO { mat4 proj; } camera;
```
The projection matrix is an orthographic matrix computed from zoom level and pan position using GLM.

**Command buffer recording:**
1. Clear to dark blue background
2. Draw triangle (demo, always present)
3. Draw fill polygons (below lines)
4. Draw line features (on top)

### 5. Shaders

Six GLSL shaders compiled to SPIR-V at build time.

| Shader | Stage | Purpose |
|--------|-------|---------|
| `triangle.vert` | Vertex | Hardcoded triangle positions via `gl_VertexIndex` |
| `triangle.frag` | Fragment | Passthrough color |
| `line.vert` | Vertex | Per-vertex position + camera transform |
| `line.frag` | Fragment | Passthrough color |
| `fill.vert` | Vertex | Per-vertex position + camera transform |
| `fill.frag` | Fragment | Passthrough color with alpha |

**Push constants:**
- Triangle/line: `vec3 color` (RGB only, no alpha)
- Fill: `vec4 fillColor` (RGBA with opacity)

### 6. Camera and Interaction

**Input handling (SDL2 events):**
- Arrow keys: pan the camera
- `+`/`-`: zoom in/out (clamped to zoom level 2-18)
- Left mouse drag: pan
- Mouse wheel: zoom
- ESC: quit

**Projection matrix:** Orthographic projection computed with GLM based on current zoom level and camera position. The camera position is in tile-local coordinates, and the zoom level controls the visible area.

## Memory Management

- **Host-visible memory** is used for vertex/index buffers (allocated with `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`)
- Data is uploaded via `vkMapMemory`/`vkUnmapMemory` once at pipeline creation
- **Uniform buffer** for the camera is also host-visible, updated every frame
- No staging buffers (this is a simplified implementation)

## File Structure

```
src/
├── main.cpp                 # Full Vulkan application (~1300 lines)
├── mvt_parser.h             # MVT PBF parser (header-only)
├── render_data.h            # Geometry → vertex/index conversion (header-only)
├── style_engine.h           # JSON style parser (header-only)
├── test_mvt.cpp             # Standalone MVT parser test
└── shaders/
    ├── triangle.vert/.frag  # Demo triangle
    ├── line.vert/.frag      # Line rendering
    └── fill.vert/.frag      # Polygon fill rendering
data/
├── style.json               # Default style definition
└── *.mvt                    # MVT tile files
```

## Data Flow: End to End

```
1. Load MVT file (data/zurich.mvt)
   └─→ mvt::parse_tile() → Tile (vector of Layer)

2. Load style (data/style.json)
   └─→ StyleEngine::loadFromJson() → StyleEngine

3. Extract geometry
   └─→ extract_lines() / extract_polygons() → LineBatch / PolyBatch

4. Match style rules
   └─→ StyleEngine::matchRule("roads", LINESTRING) → StyleRule

5. Create Vulkan pipelines
   └─→ Load SPIR-V shaders, create graphics pipeline

6. Upload vertex/index data
   └─→ vkMapMemory → vertex/index buffers

7. Render loop
   ├─→ Update camera UBO (projection matrix)
   ├─→ Record command buffers
   │   ├─→ Clear → Draw triangle
   │   ├─→ Draw fills
   │   └─→ Draw lines
   └─→ Present
```

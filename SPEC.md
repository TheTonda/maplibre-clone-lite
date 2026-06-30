# Map Renderer v2 — Specification for Ornith Agent

## Goal
Build a GPU-accelerated vector tile map renderer in C++23 with Vulkan.
Inspired by MapLibre GL Native but limited to basic features.
No external graphics libraries beyond Vulkan, GLM, SDL2, and protobuf.

## Architecture

### Data Flow
```
MVT Vector Tile (PBF) → TileDecoder → Geometry Buffers → Vulkan Pipeline → Framebuffer
JSON Style Sheet → StyleParser → Uniforms/Bindings ──────────────┘
```

### Components (in order of implementation)

1. **Tile Loader** (`tile.h`, `tile.cpp`)
   - Parse MVT (Mapbox Vector Tile) using protobuf
   - Extract layers, features, geometries (points, lines, polygons)
   - Decode zigzag-encoded coordinates
   - Output: vector of RenderFeature structs

2. **Style Engine** (`style.h`, `style.cpp`)
   - Parse a simplified JSON style (subset of MapLibre style spec)
   - Support: background color, line color/width, fill color/opacity
   - Style rules match on layer name and feature properties
   - Output: material parameters per layer

3. **Projection** (`projection.h`)
   - Header-only Mercator projection math
   - Lat/lon → pixel coordinates for given zoom level
   - Viewport transform (world coords → NDC → clip space)

4. **Renderer** (`renderer.h`, `renderer.cpp`)
   - Vulkan initialization (instance, device, swapchain via SDL2)
   - Vertex buffers for line and fill geometry
   - Render passes: background → fills → lines
   - Camera: pan and zoom via keyboard/mouse

5. **Main** (`main.cpp`)
   - Parse command-line: ./map-renderer <tile.mvt> <style.json>
   - Or interactive mode with SDL2 window

## Constraints
- C++23 (g++)
- Only dependencies: Vulkan, GLM (header-only math), SDL2, protobuf
- CMake build system
- No external graphics libraries (no bgfx, no nanovg, etc.)
- No OSM XML parsing (use pre-downloaded MVT tiles)
- Minimal: ~1000-2000 lines total

## Key Design Decisions

### Why Vulkan directly (not OpenGL)?
- Modern API, better AMD GPU support
- Explicit control over memory and synchronization
- Good learning opportunity

### Why MVT (not OSM XML)?
- Efficient binary format
- Tiled (z/x/y) — no need for massive file parsing
- Industry standard for web maps

### MVT Geometry Handling
- Lines → line list (VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
- Polygons → triangle fan (after tessellation to triangles)
- Points → point list (small quads or sprites)

### Coordinate System
- Tile-local coordinates (0-4096 for extent 4096)
- Transform to normalized device coordinates via uniform buffer
- Camera matrix for pan/zoom

## Milestones
1. ✅ Project skeleton (CMake + main.cpp stub)
2. MVT parser reads a tile and prints feature counts
3. Vulkan triangle rendered on screen
4. Parse JSON style and apply colors
5. Render MVT lines from a real tile
6. Render MVT polygons with fills
7. Interactive pan and zoom

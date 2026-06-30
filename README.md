# map-renderer-v2 — GPU-accelerated vector tile renderer

A from-scratch map renderer inspired by MapLibre GL Native, targeting basic features:
GPU rendering (Vulkan), MVT vector tiles, JSON styles, glyph rendering.

## Architecture (planned)

```
GPU Renderer (Vulkan)
  ├── Tile Loader (MVT/PBF → geometry)
  ├── Style Engine (JSON → GPU uniforms/buffers)
  ├── Glyph Atlas (SDF text rendering)
  └── Camera (Mercator projection, zoom/pan)
```

## Dependencies

- Vulkan SDK (vulkan-headers, vulkan-icd-loader, glslang)
- Protocol Buffers (protobuf, for MVT parsing)
- GLM (OpenGL Mathematics, header-only)
- SDL2 (windowing)

## Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## Status

Project skeleton — not yet implemented.

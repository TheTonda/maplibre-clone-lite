# Task 7: Camera UBO, Shader Compilation, First Shaders

**Date:** 2026-07-02
**Model:** Hermes direct write

## Summary

Created the CameraUBO struct, shader compilation script, and first GLSL shaders.

## Files Created

| File | Description |
|------|-------------|
| `src/core/camera_ubo.h` | CameraUBO struct (proj + view mat4), layout/buffer helpers |
| `tests/test_camera_ubo.cpp` | 3 tests for size, layout binding, buffer info |
| `tools/compile_shaders.sh` | Compiles all .vert/.frag to .spv via glslc |
| `src/shaders/2d/fill.vert` | 2D fill shader, vec2 position → clip space via UBO |
| `src/shaders/2d/fill.frag` | Pass-through colour from push constant |
| `src/shaders/2d/ground.vert` | Ground plane vertex shader (world pos forwarding) |
| `src/shaders/2d/ground.frag` | Dark ground with grid lines via fwidth anti-aliasing |

## Verification

```
bash tools/compile_shaders.sh   → 4 .spv files
cd _build && cmake .. && make   → 0 errors, 0 warnings
./map-renderer-tests             → 6/6 passed (3 new CameraUBO tests)
```

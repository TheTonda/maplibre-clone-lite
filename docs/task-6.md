# Task 6: Graphics Wrappers

**Date:** 2026-07-02
**Model:** Hermes direct write

## Summary

Created four reusable Vulkan graphics wrapper classes: Buffer, Descriptor, ShaderManager, and PipelineManager.

## Files Created

| File | Description |
|------|-------------|
| `src/graphics/buffer.h` | Buffer wrapper with device-local + host-visible support |
| `src/graphics/buffer.cpp` | Full staging-buffer upload path |
| `src/graphics/descriptor.h` | Descriptor set + layout + pool in one RAII package |
| `src/graphics/descriptor.cpp` | Create from bindings, update uniform buffers |
| `src/graphics/shader.h` | SPIR-V shader loading from file/memory |
| `src/graphics/shader.cpp` | File reading + module creation |
| `src/graphics/pipeline.h` | PipelineConfig struct + PipelineManager |
| `src/graphics/pipeline.cpp` | Graphics pipeline creation with all state |

## Key Details

- **Buffer**: stores device+physical_device, supports direct memcpy for host-visible and full staging-buffer path for device-local. Move-constructable.
- **Descriptor**: creates layout, pool, and set in one call. Supports update_buffer for UBO binding updates.
- **ShaderManager**: loads .spv files, creates modules, tracks them for cleanup.
- **PipelineManager**: PipelineConfig has sensible defaults (fill, back-cull CCW, no depth, no blending). Creates layouts + pipelines, stores by name. Dynamic viewport/scissor.

## Verification

```
cd _build && cmake .. && make -j$(nproc)
Build: 0 errors, 0 warnings
Tests: 3/3 pass (unchanged)
```

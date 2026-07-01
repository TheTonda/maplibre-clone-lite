# Task 5: Sync Objects and Main Render Loop

**Date:** 2026-07-02
**Model:** Hermes direct write

## Summary

Added per-frame-in-flight double-buffered sync objects (2 fences + 4 semaphores) and the frame acquisition/submission pipeline. The main loop now acquires images, submits cleared frames, and presents — producing a visible solid-colour window.

## Changes

| File | Description |
|------|-------------|
| `src/core/vulkan_context.h` | Added sync objects, command buffer helpers, acquire_next_image(), submit_frame() |
| `src/core/vulkan_context.cpp` | Implemented create_sync_objects, allocate/buffer helpers, acquire/present |
| `src/main.cpp` | Full main loop: poll events → acquire → submit → present |

## Key Details

- **Double-buffered**: `kMaxFramesInFlight = 2` — CPU runs 1 frame ahead of GPU
- **Fences**: signalled on submit, waited on acquire (CREATED_SIGNALED)
- **Semaphores**: image_available (image acquired), render_finished (render complete)
- **Clear colour**: dark grey `(0.06, 0.06, 0.07, 1.0)` — colour + depth cleared
- **Out-of-date swapchain**: handled by skipping the frame (returns ~0u)
- **Command buffer**: allocated+freed per frame for simplicity (not recycled)
- **Move semantics**: all new members properly transferred in move ctor/operator=

## Verification

```
cd _build && cmake .. && make -j$(nproc) && ./map-renderer-tests
Build: 0 errors, 0 warnings
Tests: 3/3 passed
map-renderer binary: links against SDL2 + Vulkan + protobuf
```

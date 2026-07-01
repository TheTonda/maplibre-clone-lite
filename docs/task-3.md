# Task 3: Vulkan Window Foundation

**Date:** 2026-07-02
**Model:** Hermes direct writes (intended for GLM-5.2 via OpenCode, but shell escaping issues)

## Summary

Created the SDL window abstraction, input state polling system, and Vulkan instance + device creation.

## Files Created / Modified

| File                          | Description |
|-------------------------------|-------------|
| `src/core/input_state.h`      | Per-frame input struct with keyboard/mouse/time fields and reset |
| `src/core/window.h`           | SDL2 window class with Vulkan surface |
| `src/core/window.cpp`         | Event polling: keyboard, mouse, scroll, frame timing |
| `src/core/vulkan_context.h`   | Vulkan instance + physical/logical device management |
| `src/core/vulkan_context.cpp` | Instance, debug messenger, GPU selection, device creation, swapchain query |
| `tests/test_input_state.cpp`  | Tests: default state, frame state reset (2 tests) |
| `src/main.cpp`                | Updated with Window + VulkanContext + input loop |

## Key Design Decisions

- **Window**: owns SDL_Window + VkSurfaceKHR. Surface is passed to VulkanContext.
- **InputState**: reset_frame_state() clears single-frame flags (mouse_pressed, deltas, scroll) while persistent state (mouse_down, keyboard) stays. Caller must call this each frame.
- **Validate layers**: enabled in debug builds, but does NOT crash if unavailable — just warns and continues.
- **GPU selection**: prefers discrete GPU, falls back to integrated, fails if neither has swapchain extension.
- **Window title**: "Map Renderer", default 1280x720, resizable.
- **Swapchain support**: queried from physical device and accessible via static method.
- **Move semantics**: VulkanContext is move-constructable and move-assignable (no raw copy).

## Verification

```
cd _build && cmake .. && make -j$(nproc) && ./map-renderer-tests
```

- Build: 0 errors, 0 warnings (GCC 16.1.1)
- Tests: 3/3 passed (DummyTest + InputStateTest)
- Link: libvulkan.so.1, libSDL2-2.0.so.0, libprotobuf.so.35.0.0

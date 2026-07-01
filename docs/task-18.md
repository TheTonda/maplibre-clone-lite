# Task 18: Performance & Polish

**Date:** 2026-07-02
**Model:** deepseek-v4-flash

## Summary

Final polish pass: swapchain resize handling, FPS counter in window title,
and general cleanup.

## Changes

### Swapchain Resize
- `Window`: added `resized_` flag, `was_resized()`, `reset_resized()`, `set_title()`
- `Window::poll_events`: handles `SDL_WINDOWEVENT_RESIZED` → updates width/height + sets flag
- `VulkanContext::recreate_swapchain(Window&)`: destroys and recreates swapchain,
  image views, depth resources, render pass, and framebuffers
- `main.cpp`: checks `window.was_resized()` each frame, calls `recreate_swapchain`
  when needed. Also handles `VK_ERROR_OUT_OF_DATE_KHR` from `acquire_next_image`.

### FPS Counter
- `main.cpp`: tracks frames per second, updates window title every 1s
  (e.g. "Map Renderer — 1440 FPS")
- Uses `SDL_GetTicks64()` for timing

## Files Changed
- `src/core/window.h` — `resized_`, `was_resized()`, `reset_resized()`, `set_title()`
- `src/core/window.cpp` — Resize event handling, `set_title` implementation
- `src/core/vulkan_context.h` — `recreate_swapchain` declaration
- `src/core/vulkan_context.cpp` — Full swapchain recreation logic
- `src/main.cpp` — FPS counter, resize handling in main loop

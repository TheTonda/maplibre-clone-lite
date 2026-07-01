# Task 4: Swapchain, Depth Buffer, Render Pass, Framebuffers

**Date:** 2026-07-02
**Model:** Hermes direct write

## Summary

Extended VulkanContext with swapchain creation, depth buffer, render pass, framebuffers, and command pool.

## Changes

| File | Change |
|------|--------|
| `src/core/vulkan_context.h` | Added swapchain, depth, render pass, framebuffer, command pool members and methods |
| `src/core/vulkan_context.cpp` | Implemented all new methods + static helper functions for image/memory/view creation |

## Key Design Decisions

- **Swapchain format**: B8G8R8A8_SRGB with SRGB nonlinear color space
- **Present mode**: MAILBOX (triple-buffering) with FIFO fallback
- **Depth format**: D32_SFLOAT preferred, with D32_S8 and D24_S8 fallbacks
- **Image count**: minImageCount + 1 (triple-buffering where supported)
- **Render pass**: color + depth attachments, single subpass, external→subpass dependency for color+early fragment tests
- **Depth initial/final layout**: UNDEFINED → DEPTH_STENCIL_ATTACHMENT_OPTIMAL (transitions happen automatically at subpass boundaries)
- **Command pool**: RESET_COMMAND_BUFFER flag for per-frame reuse
- **Cleanup**: reverse-order destruction with device idle before any teardown

## Verification

```
cd _build && cmake .. && make -j$(nproc) && ./map-renderer-tests
Build: 0 errors, 0 warnings
Tests: 3/3 passed
```

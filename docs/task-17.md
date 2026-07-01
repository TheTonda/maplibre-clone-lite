# Task 17: Integration Tests, Sanitizers, Documentation

**Date:** 2026-07-02
**Model:** deepseek-v4-flash

## Summary

Added integration test scaffolding, sanitizer build options, and completed
the project documentation.

## Files
- `tests/test_integration.cpp` — Full Vulkan lifecycle test (excluded by default)
- `CMakeLists.txt` — `MAP_RENDERER_USE_ASAN`, `MAP_RENDERER_USE_UBSAN`,
  `MAP_RENDERER_ENABLE_INTEGRATION_TESTS` options
- `README.md` — Updated controls, build instructions, sanitizer docs,
  project structure

## Integration Test (`tests/test_integration.cpp`)
- Creates Window → VulkanContext → Renderer → uploads ground geometry
- Runs one frame: poll events, update camera, submit, present
- Disabled by default (`-DMAP_RENDERER_ENABLE_INTEGRATION_TESTS=ON`)
- Requires physical display + GPU with Vulkan support

## Sanitizer Options
- `-DMAP_RENDERER_USE_ASAN=ON` — AddressSanitizer (-fsanitize=address)
- `-DMAP_RENDERER_USE_UBSAN=ON` — UndefinedBehaviour (-fsanitize=undefined)

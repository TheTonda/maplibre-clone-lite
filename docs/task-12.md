# Task 12: Camera System

**Date:** 2026-07-02
**Model:** deepseek-v4-flash (routine C++)

## Summary

Implemented a dual-mode camera system (2D orthographic / 3D perspective) that
serves as the view transformation for all subsequent rendering work.

## Files
- `src/core/camera.h`  — Camera interface (90 lines)
- `src/core/camera.cpp` — Full implementation (168 lines)
- `tests/test_camera.cpp` — 15 unit tests

## Features
- **2D mode**: orthographic projection centred on (x, z) at configurable zoom
  level. Aspect-ratio-aware with minimum 50m visibility. lookAt with upward
  vector pointing north (negative Z).
- **3D mode**: perspective projection (60° FOV) with spherical coordinates
  (distance, tilt, rotation) around look-at point. Full 360° orbit.
- **Mode switching**: preserves visual scale when switching (2D↔3D).
- **Clamping**: zoom/distance/tilt all clamped to sensible limits.
- **Input handling**: arrow keys for pan, mouse drag for pan (2D) / orbit (3D),
  scroll-wheel zoom, tilt/rotation keys.
- **Frame bounds**: auto-centres on data extents, clamps position within
  a 10% margin.

## Tests (15, all passing)
- Default 2D mode, frame bounds centring, mode switching preserve zoom
- Zoom/distance/tilt clamping to limits, rotation wrapping
- `zoom_by` scale factor, 2D/3D projection matrix validity
- View matrix verification (centre → origin in view space)
- Position get/set, mode switch persistence

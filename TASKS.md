# Implementation Task Checklist

Tracking implementation progress against spec/TASKS.md.
Each task maps 1:1 to spec tasks.

**Status:** 🔴 pending | 🟡 in_progress | 🟢 done  
**Commit convention:** `feat(<task-id>): <description>`

---

## Phase 1: Engine Foundation

| # | Status | Task | Commit |
|---|--------|------|--------|
| 1 | 🟢 | CMake build system + project structure | 2eeda08 |
| 2 | 🟢 | Platform abstraction interface | ca617b3 |
| 3 | 🟢 | Tile ID + OSM types | 30cc13a |
| 4 | 🟢 | Color table | 3c62239 |

## Phase 2: Data Pipeline

| # | Status | Task | Commit |
|---|--------|------|--------|
| 5 | 🟢 | Protobuf tile schema | dc261eb |
| 6 | 🟢 | Python preprocessor | 2954e6e |
| 7 | 🟢 | OSM loader + tile loader (C++) | 89581f7 |

## Phase 3: Rendering Core

| # | Status | Task | Commit |
|---|--------|------|--------|
| 8 | 🟢 | Geometry builder | cbffc70 |
| 9 | 🟢 | Shader program | 131b2df |
| 10 | 🟢 | Renderer — GL initialization + tile upload | f5a747d |
| 11 | 🟢 | Renderer — draw loop | 2a4e806 |
| 12 | 🟢 | Desktop app — window + single-tile render (SDL2) | b5f91be |

## Phase 4: Tiling + Camera

| # | Status | Task | Commit |
|---|--------|------|--------|
| 13 | 🟢 | Camera (2D orthographic) | 1574ad1 |
| 14 | 🟢 | Tile zoom selection + visible tile computation | 1574ad1 |
| 15 | 🔴 | Tile cache (LRU, thread-safe) | - |
| 16 | 🔴 | Background tile loading thread | - |
| 17 | 🔴 | Engine orchestrator + desktop app integration | - |

## Phase 5: Android App

| # | Status | Task | Commit |
|---|--------|------|--------|
| 18 | 🔴 | Android Studio project setup | - |
| 19 | 🔴 | Android platform implementation (EGL + GLES) | - |
| 20 | 🔴 | Android touch input | - |
| 21 | 🔴 | Tile data deployment to device | - |
| 22 | 🔴 | Android app — full render test | - |

## Phase 6: Testing + Optimization

| # | Status | Task | Commit |
|---|--------|------|--------|
| 23 | 🔴 | Full test suite | - |
| 24 | 🔴 | Performance profiling + optimization | - |
| 25 | 🔴 | India dataset benchmark | - |
| 26 | 🔴 | Documentation | - |
| 27 | 🔴 | Cleanup + final verification | - |
| 28 | 🔴 | Release preparation | - |

---

**Overall Status:** 14/28 done
**Last updated:** 2026-07-03

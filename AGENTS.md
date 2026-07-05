# AGENTS.md — guide for agentic sessions in this repo

This repo is **map-renderer-v2**: an offline map renderer with a C++ engine
shared between a FLTK Linux desktop app and a Kotlin+JNI Android app. A
host-side C++ compiler `mapbake` turns raw `.osm.pbf` extracts into the WebP
raster `.mbtiles` the engine consumes.

Before making changes, read `docs/PLAN.md` for the full design. This file is
the quick reference for *how to work in this repo*.

## Locked decisions (do not relitigate without explicit user approval)

- Engine: C++, exposed via a flat C ABI (`engine/include/maprender/c_api.h`).
- Map data on disk: **WebP raster `.mbtiles`** (plain `tiles(z,x,y,tile_data)`
  schema, **TMS** row orientation). The engine does **not** read vector tiles.
- Compiler `mapbake`: C++ with `libosmium`; ingests raw `.osm.pbf`; hardcoded
  OSM tag palette (no JSON styles); single user-selected zoom range (default
  z8..z14); runs as a separate host-side command (never inside the app).
- Renderer: CPU blit to a 2D canvas; **nearest-neighbour** between integer
  zooms (blocky is fine for MVP).
- Desktop: FLTK, **Linux only** for now.
- Android: API 29+ (Android 10+), Kotlin UI + **hand-written JNI**.
- MVP scope: **view + pan/zoom only**. No labels, search, routing, GPX,
  rotate/tilt, vector styles, online tiles.

The existing OpenMapTiles vector file at
`/home/tonda/Downloads/osm-2020-02-10-v3.11_india_new-delhi.mbtiles` is **not
usable by the engine** and must not be edited. Use a raw `osm.pbf` with
`mapbake` instead.

## Repository layout (see also docs/PLAN.md)

```
engine/      libmaprender (C++ engine, flat C ABI in include/maprender/c_api.h)
compiler/    mapbake CLI: osm.pbf -> WebP raster .mbtiles
desktop/     FLTK Linux viewer
android/     Kotlin + JNI viewer (Gradle NDK build)
third_party/ vendored sqlite amalgamation, libwebp, libosmium, fltk
docs/PLAN.md full plan
```

If you add a new module, place it in the subfolder matching its role. Do not
create top-level dirs other than the four artefact dirs + `third_party/` +
`docs/`.

## Build commands

All C/C++ builds are CMake-driven.

### Engine + compiler + desktop (Linux)
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
# artefacts:
#   build/engine/libmaprender.a
#   build/compiler/mapbake
#   build/desktop/mapview
```

### Engine unit tests
```bash
ctest --test-dir build --output-on-failure
```

### Android
The Android build is invoked by Gradle from `android/`. It uses
`externalNativeBuild` to compile the same `engine/` sources plus
`android/app/src/main/cpp/engine_jni.cpp` into `libmaprender.so`.
```bash
./gradlew :app:assembleDebug           # from android/
```
`abiFilters`: `arm64-v8a`, `x86_64`. minSdk 29.

### Compiler usage
```bash
./build/compiler/mapbake \
    -i region.osm.pbf \
    -o region.webp.mbtiles \
    -z 8 -Z 14
```

## Lint / typecheck / test commands

There is no project-wide linter yet. Run these before considering work done:

- `cmake --build build -j` — must compile cleanly with `-Wall -Wextra -Werror`.
- `ctest --test-dir build --output-on-failure` — GoogleTest unit tests for
  engine viewport math, TMS flip, LRU bounds, rasterizer snapshot.
- For desktop: launch `build/desktop/mapview`, open a WebP `.mbtiles`, pan
  with drag, zoom with wheel, resize window. No crashes, no flicker beyond the
  expected nearest-neighbour blockiness.
- For Android: `./gradlew :app:lint :app:assembleDebug`, install on an
  Android 10+ device, open a WebP `.mbtiles` via the system file picker.

If you add a test, run `ctest` and confirm it passes. Do not mark a phase
complete in `docs/PLAN.md` until the relevant gates pass.

## Code conventions

- **C++ standard**: C++17. Headers use `#pragma once`.
- **Naming**: `PascalCase` for classes/structs, `snake_case` for functions
  and variables, `MR_`/`kCamelCase` for C-API symbols and constants.
- **ABI hygiene**: `-fvisibility=hidden -fPIC`. Only functions declared in
  `engine/include/maprender/c_api.h` are exported. Engine internals stay in
  the `maprender::` namespace and are not exposed across the engine boundary.
- **No exceptions across the C ABI**: `c_api.cpp` wraps engine calls in
  `try/catch` and sets `last_error` instead. Public ABI functions never throw.
- **Comments**: do not add comments unless requested. Self-documenting code
  only.
- **Third-party**: vendor into `third_party/` (sqlite amalgamation, libwebp
  for NDK). Prefer system packages on Linux via `find_package` for libosmium,
  fltk, libwebp, zlib.
- **No secrets, no telemetry, no online fetches.** This is an offline
  renderer. Do not add network code.

## Coordinate conventions (memorise these — they cause silent bugs)

- **Web Mercator / EPSG:3857**, slippy-map convention.
- World width at zoom `z`: `W(z) = 256 * 2^z` pixels.
- `px = (lon + 180) / 360 * W(z)`
- `py = (1 - asinh(tan(lat_rad)) / π) / 2 * W(z)`
- **MBTiles stores TMS rows** (row 0 at the bottom). The engine flips to
  slippy on read: `slippy_row = (1 << z) - 1 - tms_row`. The compiler writes
  TMS rows. Get this wrong and the map renders upside down.

## Where things live (common lookup)

- Public ABI: `engine/include/maprender/c_api.h`
- ABI impl wrap: `engine/src/c_api.cpp`
- MBTiles read: `engine/src/mbtiles_reader.cpp`
- WebP decode: `engine/src/webp_decoder.cpp`
- LRU cache: `engine/src/tile_cache.cpp`
- Viewport math: `engine/src/viewport.cpp`
- Renderer: `engine/src/renderer.cpp`
- PBF parsing: `compiler/src/osm_reader.cpp`
- Tag→color palette: `compiler/src/style.cpp`
- Scanline rasterizer: `compiler/src/tile_rasterizer.cpp`
- MBTiles write: `compiler/src/mbtiles_writer.cpp`
- Desktop UI host: `desktop/src/MapWidget.cpp`, `desktop/src/main.cpp`
- Android JNI: `android/app/src/main/cpp/engine_jni.cpp`
- Android UI: `android/app/src/main/java/.../MapView.kt`, `MainActivity.kt`

## Phasing gates

The plan in `docs/PLAN.md` has six phases with explicit verification gates.
Do not start the next phase until the current phase's gate passes:

1. Phase 1 — engine opens an `.mbtiles` and dumps one tile as PNG.
2. Phase 2 — engine composites a viewport frame and unit tests pass.
3. Phase 3 — `mapbake` produces an `.mbtiles` that Phase 2 can render.
4. Phase 4 — desktop FLTK app opens, pans, zooms, resizes without crashes.
5. Phase 5 — Android app does the same, manually verified on API 29+.
6. Phase 6 — polish, README, cache tuning, optional prefetch thread.

## Do NOT

- Do not commit large data files (`.mbtiles`, `.osm.pbf`, generated PNGs).
  They live under `data/` which is gitignored.
- Do not add a vector-tile reader, MapLibre wrapper, GPU renderer, labels,
  routing, search, or network code without explicit user approval — these are
  out of MVP scope by design.
- Do not modify the OpenMapTiles vector file in `/home/tonda/Downloads/`.
- Do not add Python, Node, or web-tech tooling; the compiler is C++ only.
- Do not use `cd <dir> && <cmd>` in tool calls; use the `workdir` parameter.

## Creating a test fixture

A real WebP `.mbtiles` is only available after Phase 3. Until then, for
engine smoke tests, hand-craft a tiny fixture:

1. Use `stb_image_resize` or just draw a 256×256 RGBA8 buffer in C++ (solid
   colours per tile).
2. Encode each buffer with `WebPEncodeRGBA`.
3. Insert into a sqlite db with the plain `tiles(z,x,y,tile_data)` schema at
   a single zoom level (e.g. z=8, the four tiles covering New Delhi).
4. Set `metadata`: `format=webp`, `minzoom=8`, `maxzoom=8`,
   `bounds=76.692,28.183,77.733,28.969`, `scheme=tms`, `name=test`.

This fixture is committed under `engine/tests/fixtures/` (small enough for
git).
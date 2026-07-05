# map-renderer-v2 — Plan

Offline map renderer: a C++ engine shared between a FLTK Linux desktop app
and a Kotlin+JNI Android (API 29+) app. A host-side C++ compiler `mapbake`
turns raw `.osm.pbf` extracts into the WebP raster `.mbtiles` the engine
consumes.

## Goal

Two shipped artefacts on top of one small C++ engine:

1. **`mapbake`** — host-side compiler that turns a raw `.osm.pbf` extract into
   a WebP raster `.mbtiles` file.
2. **Viewer** — a simple offline map viewer that opens a local WebP `.mbtiles`
   and supports pan/zoom. Shipped as:
   - a FLTK Linux desktop app, and
   - an Android (API 29+) Kotlin app with a hand-written JNI shim over a flat
     C ABI exported from the engine.

## Locked decisions

| Area | Decision |
|---|---|
| Engine language | C++ |
| Engine ABI exposed to UIs | Flat C ABI (`engine/include/maprender/c_api.h`) |
| Map data on disk | WebP raster `.mbtiles` (plain `tiles(z,x,y,tile_data)` schema) |
| Source data the compiler ingests | Raw `.osm.pbf` (Geofabrik extracts etc.) |
| Renderer | CPU blit to a 2D canvas |
| Sub-integer zoom | Nearest-neighbour (blocky but easy) |
| Compiler language | C++ using `libosmium` |
| Compiler styling | Hardcoded OSM tag palette in C++ (no JSON) |
| Compiler output zoom strategy | Single user-selected fixed range (default z8..z14) |
| Bake location | Separate host-side command (not inside the app) |
| Desktop UI | FLTK (Linux only in MVP) |
| Android binding | Hand-written JNI (Kotlin UI) |
| Android min SDK | API 29 (Android 10+) |
| MVP feature scope | View + pan/zoom only (no labels, search, routing, GPX) |
| Desktop OSes | Linux only for now; Windows/macOS deferred |

Note: the existing OpenMapTiles vector file at
`/home/tonda/Downloads/osm-2020-02-10-v3.11_india_new-delhi.mbtiles`
is **not** directly usable by the engine — it is gzip PBF vector tiles. The
compiler takes a raw `osm.pbf` instead.

## Repository layout

```
map-renderer-v2/
├── CMakeLists.txt                # top-level; add_subdirectory for engine, compiler, desktop
├── AGENTS.md                     # repo guide for agentic sessions
├── docs/PLAN.md                  # this file
├── engine/                       # libmaprender (static on desktop, .so on Android)
│   ├── include/maprender/
│   │   ├── c_api.h               # flat C ABI used by desktop AND JNI
│   │   └── types.h
│   ├── src/
│   │   ├── c_api.cpp             # wraps classes into extern "C" functions
│   │   ├── mbtiles_reader.cpp    # plain raster MBTiles read (tiles table)
│   │   ├── webp_decoder.cpp      # libwebp → RGBA8
│   │   ├── tile_cache.cpp        # LRU by (z,x,y), RAM-bounded
│   │   ├── viewport.cpp          # Web Mercator math, pan/zoom (nearest)
│   │   └── renderer.cpp          # compose visible tiles → RGBA8 frame
│   └── tests/
├── compiler/                     # mapbake CLI tool
│   ├── src/
│   │   ├── main.cpp              # arg parsing -i/-o/-z/-Z
│   │   ├── osm_reader.cpp        # libosmium handler collecting ways/areas
│   │   ├── geometry_clip.cpp     # Sutherland–Hodgman + Cohen–Sutherland
│   │   ├── tile_baker.cpp        # iterate output tiles, gather geometry, clip
│   │   ├── tile_rasterizer.cpp   # scanline fill + thick DDA line → 256² RGBA8
│   │   ├── style.cpp             # hardcoded tag→color/layer palette
│   │   ├── webp_encoder.cpp      # RGBA8 → WebP blob (libwebp)
│   │   └── mbtiles_writer.cpp    # sqlite3 plain schema writer
│   └── tests/
├── desktop/                      # FLTK app (Linux)
│   ├── src/main.cpp              # window, menu, file picker
│   └── src/MapWidget.cpp         # Fl_Widget subclass hosting the canvas
├── android/                      # Kotlin + JNI
│   ├── app/src/main/cpp/engine_jni.cpp
│   ├── app/src/main/java/.../MainActivity.kt
│   ├── app/src/main/java/.../MapView.kt          # custom View: pan/pinch-zoom
│   └── build.gradle.kts
└── third_party/
    ├── sqlite amalgamation/
    ├── libwebp/                  # system pkg-config on Linux, vendored on NDK
    ├── libosmium/                # header-only (system package on Linux)
    └── fltk/                     # system package on Linux
```

## Engine public C ABI (`engine/include/maprender/c_api.h`)

```c
typedef struct MR_Context MR_Context;
typedef struct MR_Frame   MR_Frame;   /* opaque RGBA8 buffer, owned by engine */

MR_Context* mr_open        (const char* mbtiles_path); /* opens raster .mbtiles */
int         mr_min_zoom    (MR_Context* ctx);
int         mr_max_zoom    (MR_Context* ctx);
void        mr_bounds      (MR_Context* ctx, double* w, double* s, double* e, double* n);
const char* mr_last_error  (MR_Context* ctx);
void        mr_close       (MR_Context* ctx);

void        mr_set_view    (MR_Context* ctx, double lon, double lat,
                           int zoom_int, int screen_w, int screen_h);
void        mr_pan         (MR_Context* ctx, int dx_px, int dy_px);
void        mr_zoom        (MR_Context* ctx, int delta,
                           double anchor_lon, double anchor_lat);
const MR_Frame* mr_render  (MR_Context* ctx);
const uint8_t*  mr_frame_pixels (const MR_Frame* f);
```

The engine owns the frame buffer; the returned pointer stays valid until the
next `mr_render` call on the same context.

## Engine internal design

### MBTiles reading
- Expected **plain raster schema**: `tiles (zoom_level, tile_column, tile_row, tile_data)`.
- `tile_row` stored as **TMS**; engine flips to slippy: `slippy_row = (1<<z) - 1 - tms_row`.
- One prepared `SELECT tile_data FROM tiles WHERE z=? AND x=? AND y=?` reused;
  statement stored per context.

### WebP decode
- `WebPGetInfo` → `WebPDecodeRGBAInto` into a cache slot's `std::vector<uint8_t>`
  of size `256*256*4`. No double copy.
- Validation: reject tiles whose decoded dims aren't 256×256.

### Tile cache (LRU)
- Key `(z,x,y)` → RGBA8 bitmap. Bounded by **total decoded pixels**, default 64 MB.
- `std::list<CacheEntry>` + `std::unordered_map<Key, list_iterator>`. O(1)
  hit/move/evict.
- Compressed blob bytes stay in SQLite, not RAM.

### Viewport (Web Mercator / slippy convention)
- World width at zoom `z`: `W(z) = 256 * 2^z` pixels.
- `px = (lon + 180) / 360 * W(z)`; `py = (1 - asinh(tanφ)/π) / 2 * W(z)`.
- Camera: center pixel `(cx, cy)` + integer screen size `(sw, sh)`.
- `pan(dx, dy)`: `cx -= dx; cy -= dy`.
- `zoom(delta, anchor_lon, anchor_lat)`: convert anchor→pixel, change zoom,
  restore anchor pixel by adjusting center. **Nearest-neighbour**: snap
  effective zoom to the nearest integer, compute fractional scale factor
  `s = 2^(zoom - z_int)`; render tiles at `z_int` then blit scaled by `s`
  (MVP: integer nearest-neighbour only — blocky but OK).

### Renderer (CPU blit)
1. Determine visible integer-tile range from viewport.
2. For each visible tile: get RGBA8 from cache (load + decode on miss).
3. Compose into a `sw*sh*4` frame buffer:
   - Compute tile origin offset on screen (maybe fractional).
   - `memcpy` rows, clipped to screen edges.
   - Nearest-neighbour upscale when fractional zoom differs from integer zoom.
4. Return frame pointer (owned by context, reused next call).

Threading (MVP): single-threaded render on the UI thread. A prefetch worker
thread is a fast-follow optimisation, not MVP.

## Compiler (`mapbake`) design

### Invocation
```
mapbake -i region.osm.pbf -o region.webp.mbtiles [-z min_z=8] [-Z max_z=14]
```

### Pipeline
1. **OSM PBF read** via `osmium::io::Reader` with `osmium::memory::Buffer`
   streaming.
2. **Node lookup** via `osmium::index::map::FlexMem` (spills to disk for big
   inputs; India-sized extracts fit in RAM on a desktop).
3. **Feature classification** — each way/relation is classified into a layer
   code using a hardcoded C++ `StyleRule` table; first matching rule wins;
   rules ordered painter's order (areas first, then larger roads, then
   smaller). POIs/names/labels skipped in MVP (text rendering needs
   HarfBuzz/freetype).
4. **Tile iteration** — for each `z` in `[min_z, max_z]`, for each `(x, y)`
   tile in data bbox at `z`, gather features overlapping the tile bbox. MVP
   approach: stream ways once, for each way compute which tiles at each
   target zoom it touches, append to a per-tile feature array in
   `unordered_map<TileKey, vector<Feature>>` (spill to a temp bucket file
   if memory gets tight).
5. **Geometry clip**:
   - Polygons: Sutherland–Hodgman against tile pixel rect `[0,256)`.
   - Lines: Cohen–Sutherland segment-by-segment.
   - Coordinates: Web Mercator world px at `z`, subtract tile origin → tile px.
6. **Rasterize** per tile (256×256 RGBA8):
   - Sort features by `layer`, then area-vs-line ordering.
   - Polygons: scanline fill.
   - Lines: thick DDA (no AA for MVP); width scales with zoom.
   - Background: solid `#f5f5f3` to avoid tile seams at land edges; water
     areas fill blue.
7. **Encode WebP**: `WebPEncodeRGBA(buf, 256, 256, 256*4, 0.85)` (lossy q=0.85
   for size; switch to lossless if quality is visibly degraded).
8. **Write MBTiles**: SQLite plain schema, `INSERT INTO tiles(z,x,y,tile_data)`
   with prepared stmt, **TMS** row orientation, `metadata` rows
   `format=webp, minzoom, maxzoom, bounds, name, scheme=tms, type=baselayer`.

### Hardcoded style palette (MVP subset)

| OSM tags | Layer | Fill | Width @ z13 |
|---|---|---|---|
| natural=water / waterway | 1 | `#88bbee` fill | — |
| landuse=residential/commercial | 2 | `#e8d8c0` | — |
| landuse=forest/grass/meadow/park | 2 | `#bedead` | — |
| highway=motorway/trunk | 3 | `#d46a3a` | 4 px |
| highway=primary | 3 | `#e8922c` | 3 px |
| highway=secondary | 3 | `#fcd6a4` | 2 px |
| highway=tertiary/residential/service | 3 | `#ffffff` | 1 px |
| building | 4 | `#c0a080` filled | — |
| boundary=administrative | — | skipped in MVP | — |

## Desktop (FLTK) app

- `Fl_Window` 1024×768, menu bar: `File → Open…` invokes
  `fl_file_chooser("*.mbtiles")` then calls `mr_open`.
- Custom `Fl_Widget` subclass `MapWidget` overrides `draw()`:
  - Calls `mr_render(ctx)` then
    `fl_draw_image(mr_frame_pixels(f), 0, 0, w, h, 4)`.
- Event handling:
  - `FL_DRAG`: `mr_pan(dx, dy)` then `redraw()`.
  - `FL_MOUSEWHEEL`: `mr_zoom(Fl::event_dy(), screen_lon_at_cursor,
    screen_lat_at_cursor)`; clamp via `mr_min_zoom/max_zoom`.
  - `FL_RESIZE`: `mr_set_view(..., screen_w, screen_h)`.
- Title bar shows filename + current zoom.
- File-open errors shown via `fl_alert` with `mr_last_error(ctx)`.

## Android app (API 29+, Kotlin + JNI)

- `MainActivity`:
  - `ActivityResult` launcher for `ACTION_OPEN_DOCUMENT`.
  - On select, copy the picked `contentUri` to app-private storage (SQLite
    needs a real filesystem path the engine can open). Progress dialog during
    the one-time copy.
  - Pass the path into JNI `mr_open`.
- `MapView : View`:
  - Holds the engine context handle as a Kotlin `Long`.
  - `onDraw(canvas)`: call JNI `mrRender`; copy RGBA8 bytes into a `Bitmap`
    via `Bitmap.copyPixelsFromBuffer(ByteBuffer)` for MVP; fast-follow shares
    a direct `ByteBuffer` for zero-copy.
  - `onTouchEvent`: `ScaleGestureDetector` for pinch → `mrZoom`; pointer
    movement for pan → `mrPan`. `postInvalidateOnAnimation` for ~60fps.
  - `onSizeChanged`: call `mrSetView` with new width/height.
  - `Activity.onDestroy`: call `mr_close` to release the handle.
- JNI (`engine_jni.cpp`):
  - One `extern "C"` exported function per ABI function; converts primitive
    args; carries the pointer as `jlong`.
  - `mr_frame_pixels` returns `NewDirectByteBuffer` wrapping the engine's
    frame pointer (zero-copy read).
- Gradle `externalNativeBuild`:
  - Builds `libmaprender.so` from `engine/` sources + `engine_jni.cpp`.
  - Statically links vendored `sqlite` + `libwebp` (NDK prebuilt).
  - `abiFilters`: `arm64-v8a`, `x86_64`.

## Cross-cutting

- **CMake**: top-level creates `_engine` static target (shared only for NDK),
  `_compiler` executable (self-contained, links libosmium + sqlite + libwebp),
  `_desktop` executable (links `_engine` + FLTK). Android uses its own CMake
  invocation from Gradle consuming the same `engine/` sources.
- **Errors**: engine keeps `last_error` accessible via `mr_last_error()`;
  desktop shows it in `fl_alert`, Android throws `IllegalStateException`
  from JNI with the same string.
- **Testing**: GoogleTest for engine viewport math, TMS-flip, LRU bounds;
  rasterizer smoke-test (draw a known polygon list, snapshot-compare against
  a fixture PNG). Manual end-to-end: open baked New Delhi file in desktop +
  Android and pan/zoom.
- **ABI hygiene**: `-fvisibility=hidden -fPIC`; only `c_api.h` functions
  exported. C++ ABI seen by JNI stays stable.

## Dependencies & licenses

- SQLite amalgamation (public domain) — vendored.
- libwebp (BSD) — system on Linux, vendored for NDK.
- libosmium (BSD) — header-only, system package on Linux.
- FLTK (LGPL) — desktop only, system package on Linux.
- GoogleTest (BSD) — tests only.

## Phasing

Each phase has a verification gate before the next phase begins.

### Phase 1 — Engine smoke
- Implement `mbtiles_reader` + `webp_decoder` + a tiny CLI that opens an
  `.mbtiles` and dumps one tile as PNG (stb_image_write).
- Verify: `./engine_smoke fixtures/sample.webp.mbtiles 12 2048 2047 out.png`
  produces a sensible PNG.
- Requires a small raster fixture; we'll create one in Phase 3 (or with a
  quick hand-made fixture).

### Phase 2 — Viewport + renderer in engine
- Add viewport math, tile cache, renderer.
- CLI: load file, set view to New Delhi center at z=12, save composited frame
  to PNG; compare against a hand-checked snapshot.
- Unit tests: lat/lon ↔ pixel, TMS flip round-trip, cache eviction order.

### Phase 3 — Compiler MVP
- `mapbake` end-to-end on a small pbf extract.
- Verify: output `.mbtiles` opens with the Phase-2 CLI and renders New Delhi
  at z=12 recognisably.

### Phase 4 — Desktop FLTK
- Wire `mr_open`/`mr_render`/`mr_pan`/`mr_zoom` into `MapWidget`.
- Manual verify: open Phase-3 output, pan with drag, zoom with wheel, resize
  window, no crashes.

### Phase 5 — Android
- Set up Gradle NDK build selecting the engine sources.
- Implement `MapView`; manual verify on Android 10+ device/emulator with the
  same mbtiles pushed via `adb push`.

### Phase 6 — Polish
- Cache tuning, error surfacing, file-picker UX, README with build steps,
  optional prefetch worker thread.

## Open items / assumptions

- Linux desktop only in MVP; Windows/macOS deferred.
- Vendor libwebp + sqlite3 in `third_party/`; libosmium + fltk via system
  packages on Linux, vendored for NDK.
- The OpenMapTiles vector file at
  `/home/tonda/Downloads/osm-2020-02-10-v3.11_india_new-delhi.mbtiles` is
  **not modified** and not directly usable by the engine — the compiler
  takes a raw `osm.pbf` instead.
# Low-Level Design (LLD) Specification
## Interactive 2D Map Renderer

**Version:** 2.0  
**Date:** July 3, 2026  
**Status:** Specification — Redesigned for 2D tiled rendering on Android

---

## 1. Module Breakdown

### 1.1 Module Hierarchy

```
engine/
├── include/map_renderer/
│   ├── engine.h                # Main engine API — orchestrates all subsystems
│   ├── camera.h                # 2D orthographic camera with tile-aware zoom
│   ├── tile_id.h               # TileId struct (z, x, y) + hash for unordered_map
│   ├── tile_cache.h            # LRU cache, thread-safe
│   ├── tile_loader.h           # Background thread: file I/O → zstd → protobuf
│   ├── renderer.h              # GL rendering: VAO/VBO, shader, draw calls
│   ├── geometry_builder.h      # Triangulation, road quads
│   ├── osm_types.h             # Internal data structures (TileData, Building, Road, PolygonFeature)
│   ├── osm_loader.h            # Protobuf deserialization
│   ├── color_table.h           # Hardcoded feature → color mapping
│   ├── platform.h              # Platform abstraction interface
│   ├── debug_log.h             # DEBUG_LOG macro
│   └── gl_check.h              # GL_CHECK macro
├── src/
│   ├── camera.cpp
│   ├── tile_cache.cpp
│   ├── tile_loader.cpp
│   ├── renderer.cpp
│   ├── geometry_builder.cpp
│   ├── osm_loader.cpp
│   ├── color_table.cpp
│   └── shaders/
│       ├── fill_vert.h         # Vertex shader GLSL as raw string literal
│       └── fill_frag.h         # Fragment shader GLSL as raw string literal

desktop_app/
└── src/
    └── main.cpp                # SDL2 + GL 3.3 + GLAD + implements PlatformInterface

android_app/
├── app/src/main/jni/
│   ├── engine_jni.cpp          # JNI bridge: NativeActivity → engine
│   └── android_platform.cpp    # Implements PlatformInterface (EGL, touch, file paths)
└── ...

tools/
├── preprocess.py               # PBF → tile pyramid
├── osm_data.proto              # Tile protobuf schema
└── requirements.txt

tests/
├── test_camera.cpp
├── test_geometry_builder.cpp
├── test_tile_loader.cpp
├── test_tile_cache.cpp
└── test_color_table.cpp
```

---

## 2. Platform Abstraction

### 2.1 Platform Interface (`engine/include/map_renderer/platform.h`)

```cpp
#pragma once

#include <cstdint>    // uint32_t, int32_t, intptr_t (GLFunctions types)
#include <string>     // std::string (PlatformInterface::get_tile_data_path)

namespace map_renderer {

// GL function pointers are app-specific. The app loads them (via GLAD)
// and passes a struct of needed function pointers to the engine.
//
// IMPORTANT: This struct uses standard C++ types (uint32_t, int32_t, etc.)
// instead of GL types (GLuint, GLsizei, etc.) so that platform.h does NOT
// need to include any GL headers. This preserves the "no GL headers in
// engine public headers" rule (FR-5.3, NFR-2.1) and allows headless tests
// to compile without a GL SDK installed. The app casts GLAD-loaded
// function pointers to these signatures when filling the struct (e.g.
// reinterpret_cast<void(*)(int32_t, uint32_t*)>(glGenVertexArrays)) — the
// types have identical binary representation on all target platforms.
struct GLFunctions {
    // Core functions needed by the engine
    void (*glGenVertexArrays)(int32_t, uint32_t*);
    void (*glDeleteVertexArrays)(int32_t, const uint32_t*);
    void (*glBindVertexArray)(uint32_t);
    void (*glGenBuffers)(int32_t, uint32_t*);
    void (*glDeleteBuffers)(int32_t, const uint32_t*);
    void (*glBindBuffer)(uint32_t, uint32_t);
    void (*glBufferData)(uint32_t, intptr_t, const void*, uint32_t);
    void (*glEnableVertexAttribArray)(uint32_t);
    void (*glVertexAttribPointer)(uint32_t, int32_t, uint32_t, uint8_t, int32_t, const void*);
    void (*glDrawArrays)(uint32_t, int32_t, int32_t);
    void (*glUseProgram)(uint32_t);
    void (*glUniformMatrix4fv)(int32_t, int32_t, uint8_t, const float*);
    void (*glUniform4f)(int32_t, float, float, float, float);
    void (*glUniform2f)(int32_t, float, float);
    void (*glGetUniformLocation)(uint32_t, const char*);
    void (*glClearColor)(float, float, float, float);
    void (*glClear)(uint32_t);
    void (*glViewport)(int32_t, int32_t, int32_t, int32_t);
    uint32_t (*glCreateShader)(uint32_t);
    void (*glShaderSource)(uint32_t, int32_t, const char* const*, const int32_t*);
    void (*glCompileShader)(uint32_t);
    void (*glGetShaderiv)(uint32_t, uint32_t, int32_t*);
    void (*glGetShaderInfoLog)(uint32_t, int32_t, int32_t*, char*);
    void (*glDeleteShader)(uint32_t);
    uint32_t (*glCreateProgram)(void);
    void (*glAttachShader)(uint32_t, uint32_t);
    void (*glLinkProgram)(uint32_t);
    void (*glGetProgramiv)(uint32_t, uint32_t, int32_t*);
    void (*glGetProgramInfoLog)(uint32_t, int32_t, int32_t*, char*);
    void (*glDeleteProgram)(uint32_t);
    void (*glEnable)(uint32_t);
    void (*glDisable)(uint32_t);
    uint32_t (*glGetError)(void);  // required by GL_CHECK macro
    // ... extend as needed
};

// Input events pushed by the app into the engine
enum class InputEvent {
    PanStart, PanMove, PanEnd,
    Zoom,       // delta value
    KeyQuit,
    KeyZoomIn, KeyZoomOut,
    KeyPanLeft, KeyPanRight, KeyPanUp, KeyPanDown,
};

struct InputData {
    InputEvent type;
    float x, y;     // for pan move (screen delta), or zoom center
    float delta;    // for zoom
};

class PlatformInterface {
public:
    virtual ~PlatformInterface() = default;

    // GL functions (loaded by app via GLAD, passed to engine)
    virtual const GLFunctions& get_gl_functions() const = 0;

    // Viewport
    virtual int get_viewport_width() const = 0;
    virtual int get_viewport_height() const = 0;

    // Filesystem
    virtual std::string get_tile_data_path() const = 0;

    // Called by engine when it wants to quit
    virtual void request_quit() = 0;

    // VSync control
    virtual void set_vsync(bool enabled) = 0;
};

} // namespace map_renderer
```

### 2.2 Desktop Platform (SDL2)

```cpp
// desktop_app/src/main.cpp
class DesktopPlatform : public PlatformInterface {
    SDL_Window* window_;
    SDL_GLContext gl_context_;
    GLFunctions gl_funcs_;  // filled by GLAD loader (reinterpret_cast to
                            //   standard-type signatures — see GLFunctions)
    std::string tile_path_;

public:
    bool initialize(int width, int height, const std::string& tile_path);
    void poll_events(std::vector<InputData>& out_events);  // SDL events → InputData
    void swap_buffers();
    // ... implements all PlatformInterface methods
};
```

SDL2 setup:
```cpp
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
SDL_GL_SetSwapInterval(1);  // VSync
```

### 2.3 Android Platform (NativeActivity + EGL)

```cpp
// android_app/src/main/jni/android_platform.cpp
class AndroidPlatform : public PlatformInterface {
    ANativeWindow* window_;
    EGLDisplay display_;
    EGLSurface surface_;
    EGLContext context_;
    GLFunctions gl_funcs_;  // filled by GLAD (GLES mode)
    std::string tile_path_; // app internal storage

public:
    bool initialize(ANativeWindow* window);
    void handle_touch(AInputEvent* event, std::vector<InputData>& out);
    void swap_buffers();
    // ... implements all PlatformInterface methods
};
```

EGL setup:
```cpp
EGLint attribs[] = {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
    EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
    EGL_NONE
};
// ... choose config, create context with EGL_CONTEXT_CLIENT_VERSION 3
```

Touch handling:
```cpp
// Single finger drag → PanMove with delta
// Two finger pinch → Zoom with delta = (current_dist - prev_dist) / prev_dist
```

---

## 3. Tile System

### 3.1 Tile ID (`engine/include/map_renderer/tile_id.h`)

```cpp
namespace map_renderer {

struct TileId {
    uint32_t z;  // zoom level
    uint32_t x;  // tile column
    uint32_t y;  // tile row

    bool operator==(const TileId& other) const {
        return z == other.z && x == other.x && y == other.y;
    }

    // For use as key in unordered_map.
    // Uses hash-combine (boost::hash_combine style) so it works correctly
    // for all valid zoom levels (z up to 20+, x/y up to 2^z).
    struct Hash {
        size_t operator()(const TileId& t) const {
            size_t h = std::hash<uint32_t>{}(t.z);
            h ^= std::hash<uint32_t>{}(t.x)
                 + 0x9e3779b9u + (h << 6) + (h >> 2);
            h ^= std::hash<uint32_t>{}(t.y)
                 + 0x9e3779b9u + (h << 6) + (h >> 2);
            return h;
        }
    };
};

} // namespace map_renderer
```

### 3.2 Protobuf Schema (`tools/osm_data.proto`)

```protobuf
syntax = "proto3";
package map_renderer;

// A 2D point in per-tile local ENU meters (float32)
// x = east, z = north, relative to tile center
message Point2D {
    float x = 1;
    float z = 2;
}

// Building footprint (2D for now, height kept for future 3D)
message Building {
    int64 id = 1;
    repeated Point2D footprint = 2;
    float height_m = 3;  // kept for future 3D, unused in 2D rendering
}

// Road as a line string
message Road {
    int64 id = 1;
    repeated Point2D line = 2;
    string type = 3;     // "primary", "secondary", "residential", etc.
    float width_m = 4;   // road width in meters (fallback: 6.0)
}

// Filled polygon (park, water, landuse)
message PolygonFeature {
    repeated Point2D polygon = 1;
    string type = 2;     // "park", "water", "landuse"
}

// A single tile at (z, x, y)
message Tile {
    uint32 zoom = 1;
    uint32 tile_x = 2;
    uint32 tile_y = 3;

    // Tile center in WGS84 (double precision, for computing world offset)
    double center_lat = 4;
    double center_lon = 5;

    // Feature counts for pre-allocation
    uint32 building_count = 6;
    uint32 road_count = 7;
    uint32 polygon_count = 8;

    // Features
    repeated Building buildings = 9;
    repeated Road roads = 10;
    repeated PolygonFeature polygons = 11;  // parks, water, landuse combined

    // Simplification tolerance applied (meters)
    float simplify_tolerance = 12;
}

// Dataset metadata (one per region, zstd-compressed → metadata.bin)
message DatasetMetadata {
    string name = 1;             // "new_delhi" or "india"
    double min_lat = 2;
    double max_lat = 3;
    double min_lon = 4;
    double max_lon = 5;
    double ref_lat = 6;          // ENU reference point
    double ref_lon = 7;
    repeated uint32 zoom_levels = 8;  // [8, 12, 15, 17]
    uint64 total_tiles = 9;
}
```

### 3.3 Internal Data Structures (`engine/include/map_renderer/osm_types.h`)

```cpp
namespace map_renderer {

struct Point {
    float x, z;  // local ENU meters relative to tile center
};

struct Building {
    int64_t id;
    std::vector<Point> footprint;
    float height;  // kept for future 3D
};

struct Road {
    int64_t id;
    std::vector<Point> line;
    std::string type;
    float width;
};

struct PolygonFeature {
    std::vector<Point> polygon;
    std::string type;  // "park", "water", "landuse"
};

// NOTE: GPU resource handles use uint32_t (not GLuint/GLsizei) to keep this
// header free of OpenGL types. GLFunctions also uses standard C++ types, so
// no cast is needed when passing these handles to GL function pointers.
// This preserves the "no GL headers in engine core" rule (FR-5.3, NFR-2.1).
struct TileData {
    TileId id;
    double center_lat, center_lon;  // for computing world offset

    // CPU feature data (freed after GPU upload — see section 6.4)
    std::vector<Building> buildings;
    std::vector<Road> roads;
    std::vector<PolygonFeature> polygons;

    // Computed world offset (set by OSMLoader during deserialization)
    float world_offset_x, world_offset_z;

    // GPU resources (owned by renderer, opaque handles)
    uint32_t vao = 0;
    uint32_t vbo = 0;

    // Vertex offsets within VBO (set by geometry builder)
    // offset/count are in VERTEX units (each vertex = 2 floats = 8 bytes)
    struct DrawRange {
        uint32_t offset;
        uint32_t count;
    };
    DrawRange water_range;
    DrawRange park_range;
    DrawRange landuse_range;
    DrawRange road_range;
    DrawRange building_range;

    // True once geometry has been uploaded to GPU and CPU vectors freed
    bool uploaded = false;
};

} // namespace map_renderer
```

### 3.4 Tile Cache (`engine/include/map_renderer/tile_cache.h`)

```cpp
namespace map_renderer {

class TileCache {
public:
    explicit TileCache(size_t max_tiles = 64);

    // Called by render thread: returns tile if loaded, nullptr if not
    // Marks tile as recently used
    std::shared_ptr<TileData> get(const TileId& id);

    // Called by loader thread: inserts a newly loaded tile.
    // Records the id in an insert-log for drain_recent_inserts().
    // Evicts LRU tile if over capacity (frees GPU buffers via callback).
    void put(const TileId& id, std::shared_ptr<TileData> tile);

    // Called by render thread (Engine): returns and clears the list of
    // tile IDs inserted since the last call. The Engine uses this to know
    // which tiles need GPU upload (Renderer::on_tile_loaded) without
    // giving the loader thread a Renderer pointer.
    std::vector<TileId> drain_recent_inserts();

    // Called by render thread: mark tile for removal
    void invalidate(const TileId& id);

    // Set callback for GPU resource cleanup on eviction
    void set_eviction_callback(std::function<void(const TileId&, TileData&)> cb);

    // Stats
    size_t size() const;
    size_t capacity() const;

private:
    size_t max_tiles_;
    mutable std::mutex mutex_;
    std::unordered_map<TileId, std::shared_ptr<TileData>, TileId::Hash> cache_;
    std::list<TileId> lru_order_;  // front = most recent
    std::vector<TileId> recent_inserts_;  // appended on put(), drained by Engine
    std::function<void(const TileId&, TileData&)> eviction_cb_;

    void evict_lru();
};

} // namespace map_renderer
```

### 3.5 OSM Loader (`engine/include/map_renderer/osm_loader.h`)

**Purpose:** Pure protobuf deserialization — converts a byte buffer (already
zstd-decompressed) into a `TileData` struct. No file I/O, no threading.

```cpp
namespace map_renderer {

class OSMLoader {
public:
    // Deserialize a protobuf byte buffer into TileData.
    // Returns false on parse error (corrupt data, schema mismatch).
    // Reserves feature vectors from the tile header counts before filling
    // them, so there is no reallocation during deserialization.
    // Sets world_offset_x/z from center_lat/lon using the dataset reference
    // point (see section 5.3 for the exact ENU formula).
    static bool deserialize(const std::vector<uint8_t>& bytes,
                            const TileId& id,
                            double ref_lat, double ref_lon,
                            TileData& out);
};

} // namespace map_renderer
```

### 3.6 Tile Loader (`engine/include/map_renderer/tile_loader.h`)

**Purpose:** Background thread that reads tile files from storage, decompresses
zstd, and hands deserialized `TileData` to the `TileCache`. Delegates protobuf
parsing to `OSMLoader`.

```cpp
namespace map_renderer {

class TileLoader {
public:
    TileLoader(const std::string& tile_dir, TileCache& cache,
               double ref_lat, double ref_lon);

    // Start background thread
    void start();

    // Stop background thread (waits for current load to finish)
    void stop();

    // Called by render thread: request a set of tiles to be loaded
    void request_tiles(const std::vector<TileId>& tiles);

    // Called by render thread: cancel loads for tiles no longer needed
    void cancel_tiles(const std::vector<TileId>& tiles);

private:
    std::string tile_dir_;
    TileCache& cache_;
    double ref_lat_, ref_lon_;  // for OSMLoader world-offset computation
    std::thread worker_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::unordered_set<TileId, TileId::Hash> pending_;
    std::atomic<bool> running_{false};

    void worker_loop();
    bool load_tile(const TileId& id);

    // File path: tile_dir/z/x/y.bin (zstd-compressed protobuf)
    std::string tile_path(const TileId& id) const;
};

} // namespace map_renderer
```

**Load sequence (in `load_tile`):**
1. Read file `tile_dir/z/x/y.bin` → raw zstd bytes
2. zstd decompress → protobuf bytes
3. Call `OSMLoader::deserialize(bytes, id, ref_lat_, ref_lon_, tile_data)`
4. On success, hand `tile_data` to `TileCache::put(id, tile_data)`
5. On failure (missing/corrupt), log and drop — the Engine will retry on
   next request only if the camera still wants it.

---

## 4. Geometry Builder

### 4.1 Interface (`engine/include/map_renderer/geometry_builder.h`)

```cpp
namespace map_renderer {

struct BuiltGeometry {
    // Single VBO containing all feature vertices
    // Each vertex: (x, z) = 2 floats = 8 bytes
    std::vector<float> vertices;  // flat: x0, z0, x1, z1, ...

    // Draw ranges (offset and count in vertices, not bytes)
    TileData::DrawRange water;
    TileData::DrawRange park;
    TileData::DrawRange landuse;
    TileData::DrawRange road;
    TileData::DrawRange building;
};

class GeometryBuilder {
public:
    // Build all geometry for a tile, returns vertex data + draw ranges
    static BuiltGeometry build_tile(const TileData& tile);

private:
    // Ear-clipping triangulation for simple polygons
    // Returns triangle vertex indices
    static std::vector<uint32_t> triangulate(const std::vector<Point>& polygon);

    // Generate road quads from a line string
    // For each segment: compute perpendicular, extrude by width/2
    // Output: 2 triangles per segment (6 vertices)
    static std::vector<float> build_road_quads(
        const std::vector<Point>& line,
        float width
    );

    // Triangulate a polygon and append vertices to output
    static void append_triangulated_polygon(
        const std::vector<Point>& polygon,
        std::vector<float>& out_vertices
    );
};

} // namespace map_renderer
```

### 4.2 Ear-Clipping Triangulation

```
For a simple polygon (no holes — polygon holes are out of scope for v2.0):
1. Ensure winding is counter-clockwise (reverse if needed)
2. While polygon has > 3 vertices:
   a. For each vertex vi:
      - Check if vi is a convex vertex (cross product of prev→vi and vi→next > 0)
      - Check if no other vertex is inside triangle (prev, vi, next)
      - If both: vi is an "ear" — emit triangle (prev, vi, next), remove vi
   b. If no ear found (degenerate): emit fan triangulation as fallback
3. Emit final triangle
```

### 4.3 Road Quad Generation

```
For each segment (p0, p1) in the line string:
  dir = normalize(p1 - p0)
  perp = (-dir.z, dir.x)  // perpendicular in 2D (x,z plane)
  half_w = width / 2

  v0 = p0 + perp * half_w   // left start
  v1 = p0 - perp * half_w   // right start
  v2 = p1 + perp * half_w   // left end
  v3 = p1 - perp * half_w   // right end

  Triangle 1: (v0, v1, v2)
  Triangle 2: (v1, v3, v2)

  Append 6 vertices (2 triangles) to output
```

No shared vertices between segments (avoids miter joins — acceptable for v2.0, can add miter joins later).

---

## 5. Camera

### 5.1 Interface (`engine/include/map_renderer/camera.h`)

```cpp
namespace map_renderer {

enum class CameraMode {
    MODE_2D,  // implemented
    MODE_3D,  // reserved for future, not implemented in v2.0
};

class Camera {
public:
    Camera();

    // Position in world ENU meters (from dataset reference point)
    void set_position(float x, float z);
    void pan(float dx, float dz);

    // Zoom: visible_span = meters across the shorter viewport dimension
    void set_visible_span(float span);
    void zoom_by(float factor);

    // Get matrices (only recomputed if dirty)
    glm::mat4 get_projection_matrix(float aspect) const;
    glm::mat4 get_view_matrix() const;
    bool is_dirty() const;
    void clear_dirty();

    // Current state
    float get_x() const;
    float get_z() const;
    float get_visible_span() const;

    // Determine which tile zoom level matches the current view
    uint32_t get_tile_zoom() const;

    // Compute visible tile range at the current tile zoom
    std::vector<TileId> get_visible_tiles(uint32_t tile_zoom) const;

    // Input
    void apply_input(const InputData& input, float dt);

    // Set the ENU reference point (from dataset metadata).
    // Required before get_visible_tiles() can convert world ENU → WGS84.
    void set_reference_point(double ref_lat, double ref_lon);

    // Set dataset bounds (for initial framing and limits)
    void set_dataset_bounds(float min_x, float max_x, float min_z, float max_z);

    // Frame the entire dataset
    void frame_dataset();

private:
    float x_ = 0.0f;       // world ENU east
    float z_ = 0.0f;       // world ENU north
    float visible_span_ = 50000.0f;  // meters across shorter viewport dim

    float min_x_, max_x_, min_z_, max_z_;  // dataset bounds

    double ref_lat_ = 0.0;  // ENU reference point (from metadata)
    double ref_lon_ = 0.0;

    bool dirty_ = true;

    // Cached matrices
    mutable glm::mat4 proj_;
    mutable glm::mat4 view_;
    mutable bool matrices_valid_ = false;

    void recompute_matrices(float aspect) const;
    void clamp_position();
};

} // namespace map_renderer
```

### 5.2 Tile Zoom Selection

```cpp
uint32_t Camera::get_tile_zoom() const {
    // visible_span_ = meters across shorter viewport dimension
    // At zoom z, a tile spans approximately:
    //   tile_span = 40075000 * cos(ref_lat) / 2^z  (Web Mercator meters)
    // We want ~4-8 tiles visible across the viewport
    // So: target_tile_span ≈ visible_span / 6

    // Simpler: use lookup table
    if (visible_span_ > 500000.0f) return 8;
    if (visible_span_ > 50000.0f) return 12;
    if (visible_span_ > 5000.0f) return 15;
    return 17;
}
```

### 5.3 Visible Tile Computation

**CRITICAL: The ENU conversion formula here MUST exactly match the one used
by the Python preprocessor (§9.2).** If the two diverge, tiles will be
misaligned at India scale. Both use the equirectangular approximation
below — accurate enough for sub-country spans, identical on both sides.

```cpp
// Constants shared by preprocessor and camera:
//   R = 6371000.0  (Earth radius, meters)
//   ref_lat, ref_lon = dataset reference point (from metadata)
//
// Forward (preprocessor: WGS84 → world ENU meters):
//   x = R * cos(radians(ref_lat)) * radians(lon - ref_lon)
//   z = R * radians(lat - ref_lat)
//
// Inverse (camera: world ENU meters → WGS84):
//   lat = ref_lat + degrees(z / R)
//   lon = ref_lon + degrees(x / (R * cos(radians(ref_lat))))

std::vector<TileId> Camera::get_visible_tiles(uint32_t tile_zoom) const {
    // 1. Convert camera center (x_, z_) from world ENU to lat/lon
    //    using the inverse formula above (ref_lat_/ref_lon_ set via
    //    set_reference_point() from dataset metadata).
    // 2. Convert the viewport half-spans to lat/lon deltas using the
    //    same formula, giving the viewport's lat/lon bounding box.
    // 3. Compute tile x/y range from the bounding box:
    //    tile_x = floor((lon + 180) / 360 * 2^z)
    //    tile_y = floor((1 - ln(tan(lat) + 1/cos(lat)) / π) / 2 * 2^z)
    //    (Standard slippy map tile computation, Web Mercator)
    // 4. Clamp to [0, 2^z) and return all TileIds in the x/y range.
    //
    // NOTE: world ENU is equirectangular but tile addressing is Web
    // Mercator. The mismatch is a projection error of a few meters at
    // city scale — acceptable because per-tile geometry is stored in
    // local ENU relative to each tile's OWN center (stored as double
    // lat/lon in the protobuf header). The camera's world→lat/lon
    // conversion is only used to decide WHICH tiles to load, not to
    // position them. Tile positioning uses each tile's authoritative
    // center_lat/lon (double) → world offset, so seams are seamless.
}
```

### 5.4 Matrix Computation

```cpp
void Camera::recompute_matrices(float aspect) const {
    float half_span = visible_span_ / 2.0f;
    float half_w, half_h;

    if (aspect >= 1.0f) {
        // Landscape: span is the shorter dimension (height)
        half_h = half_span;
        half_w = half_span * aspect;
    } else {
        // Portrait: span is the shorter dimension (width)
        half_w = half_span;
        half_h = half_span / aspect;
    }

    proj_ = glm::ortho(
        x_ - half_w, x_ + half_w,
        z_ - half_h, z_ + half_h,
        -1.0f, 1.0f
    );
    view_ = glm::mat4(1.0f);  // Identity (no view transform needed for 2D)
    matrices_valid_ = true;
}
```

---

## 6. Renderer

### 6.1 Interface (`engine/include/map_renderer/renderer.h`)

```cpp
namespace map_renderer {

class Renderer {
public:
    Renderer();
    ~Renderer();

    // Initialize with platform (provides GL functions + viewport)
    bool initialize(PlatformInterface& platform);

    // Cleanup all GL resources
    void cleanup();

    // Render one frame.
    // visible_tiles is computed by the Engine (only when camera is dirty)
    // and passed in here so the render loop performs zero heap allocations.
    // Engine must keep the vector alive for the duration of this call.
    void render(const Camera& camera, TileCache& cache,
                const std::vector<TileId>& visible_tiles);

    // Called by TileCache when a tile is evicted (free GL resources)
    void on_tile_evicted(const TileId& id, TileData& tile);

    // Called by Engine (after TileLoader inserts into cache) to upload
    // a freshly loaded tile's geometry to the GPU. See section 8.2.
    void on_tile_loaded(const TileId& id, TileData& tile);

private:
    PlatformInterface* platform_;
    const GLFunctions* gl_;

    // Shader program (uint32_t — GLFunctions takes standard types, no
    // GL header needed in this public header)
    uint32_t shader_program_;
    int32_t uniform_proj_;
    int32_t uniform_view_;
    int32_t uniform_color_;
    int32_t uniform_tile_offset_;

    // Shader sources (embedded)
    bool compile_shaders();

    // Upload tile geometry to GPU
    void upload_tile_geometry(TileData& tile);

    // Draw a single tile
    void draw_tile(const TileData& tile, const glm::mat4& proj, const glm::mat4& view);

    // Color table
    glm::vec4 get_color(const std::string& feature_type) const;
};

} // namespace map_renderer
```

### 6.2 Shader Source (Embedded)

**Vertex shader** (`engine/src/shaders/fill_vert.h`):
```cpp
// Shared between desktop (#version 330 core) and Android (#version 300 es)
// Desktop app prepends "#version 330 core\n"
// Android app prepends "#version 300 es\nprecision highp float;\n"
namespace shader_source {
inline const char* fill_vertex = R"(
layout(location = 0) in vec2 a_position;    // local ENU (x, z) relative to tile center

uniform mat4 u_proj;
uniform mat4 u_view;
uniform vec2 u_tile_offset;  // world ENU offset of this tile's center

void main() {
    vec2 world_pos = a_position + u_tile_offset;
    gl_Position = u_proj * u_view * vec4(world_pos, 0.0, 1.0);
}
)";
}
```

**Fragment shader** (`engine/src/shaders/fill_frag.h`):
```cpp
namespace shader_source {
inline const char* fill_fragment = R"(
uniform vec4 u_color;
out vec4 frag_color;

void main() {
    frag_color = u_color;
}
)";
}
```

**ES compatibility notes:**
- `#version 300 es` requires precision qualifiers; the Android app prepends `precision highp float;`
- `out vec4 frag_color` works in both 330 core and 300 es
- No `gl_FragColor` (deprecated in both)
- No texture sampling (solid color fills only for v2.0)

### 6.3 Render Flow

```cpp
void Renderer::render(const Camera& camera, TileCache& cache,
                      const std::vector<TileId>& visible_tiles) {
    int vp_w = platform_->get_viewport_width();
    int vp_h = platform_->get_viewport_height();
    float aspect = float(vp_w) / float(vp_h);

    gl_->glViewport(0, 0, vp_w, vp_h);

    // Clear with ground color
    glm::vec4 ground = get_color("ground");
    gl_->glClearColor(ground.r, ground.g, ground.b, ground.a);
    gl_->glClear(GL_COLOR_BUFFER_BIT);

    // Get camera matrices (only recomputed if dirty)
    glm::mat4 proj = camera.get_projection_matrix(aspect);
    glm::mat4 view = camera.get_view_matrix();

    // Bind shader
    gl_->glUseProgram(shader_program_);
    gl_->glUniformMatrix4fv(uniform_proj_, 1, GL_FALSE, &proj[0][0]);
    gl_->glUniformMatrix4fv(uniform_view_, 1, GL_FALSE, &view[0][0]);

    // Draw each ready tile in draw order.
    // visible_tiles is provided by Engine (computed only when camera dirty)
    // — no per-frame allocation here.
    for (const TileId& tid : visible_tiles) {
        auto tile = cache.get(tid);
        if (!tile) continue;  // not loaded yet — skip (stale LOD or blank)

        // Set tile offset uniform
        gl_->glUniform2f(uniform_tile_offset_, tile->world_offset_x, tile->world_offset_z);

        // Bind tile VAO (tile->vao is already uint32_t, matching GLFunctions)
        gl_->glBindVertexArray(tile->vao);

        // Draw in order: water → parks → landuse → roads → buildings.
        auto draw_range = [&](const TileData::DrawRange& r, const glm::vec4& color) {
            if (r.count == 0) return;
            gl_->glUniform4f(uniform_color_, color.r, color.g, color.b, color.a);
            gl_->glDrawArrays(GL_TRIANGLES,
                              static_cast<int32_t>(r.offset),
                              static_cast<int32_t>(r.count));
        };

        draw_range(tile->water_range, get_color("water"));
        draw_range(tile->park_range, get_color("park"));
        draw_range(tile->landuse_range, get_color("landuse"));
        // v2.0: all roads use the generic "road" color. road_primary and
        // road_secondary colors are defined in the table for future per-type
        // rendering (requires splitting road_range by type in GeometryBuilder).
        draw_range(tile->road_range, get_color("road"));
        draw_range(tile->building_range, get_color("building"));
    }

    GL_CHECK(*gl_);
}
```

---

## 7. Color Table

### 7.1 Hardcoded Colors (`engine/include/map_renderer/color_table.h`)

```cpp
namespace map_renderer {

struct ColorEntry {
    float r, g, b, a;
};

// Hardcoded feature → color table for v2.0
// Future: replace with JSON-loaded table
inline ColorEntry get_color(const std::string& feature_type) {
    static const std::unordered_map<std::string, ColorEntry> table = {
        {"ground",         {0.12f, 0.12f, 0.14f, 1.0f}},  // dark gray
        {"water",          {0.30f, 0.55f, 0.79f, 1.0f}},  // blue
        {"park",           {0.56f, 0.73f, 0.50f, 1.0f}},  // green
        {"landuse",        {0.91f, 0.88f, 0.85f, 1.0f}},  // tan
        {"building",       {0.85f, 0.76f, 0.65f, 1.0f}},  // light brown
        {"road",           {0.95f, 0.95f, 0.95f, 1.0f}},  // white
        {"road_primary",   {1.00f, 0.98f, 0.90f, 1.0f}},  // warm white
        {"road_secondary", {0.94f, 0.94f, 0.94f, 1.0f}},  // light gray
    };
    auto it = table.find(feature_type);
    if (it != table.end()) return it->second;
    return {1.0f, 0.0f, 1.0f, 1.0f};  // magenta = unknown (debug indicator)
}

} // namespace map_renderer
```

---

## 8. Engine (Orchestrator)

### 8.1 Main Engine API (`engine/include/map_renderer/engine.h`)

```cpp
namespace map_renderer {

class Engine {
public:
    Engine();
    ~Engine();

    // Initialize with platform
    bool initialize(PlatformInterface& platform, const std::string& dataset_name);

    // Called by app each frame
    // 1. Process input events
    // 2. Update camera
    // 3. If camera dirty: recompute visible+prefetch tiles, request/cancel
    // 4. Upload any newly-loaded tiles to GPU (polls cache)
    // 5. Render
    void update(const std::vector<InputData>& input_events, float dt);

    // Called when viewport size changes
    void on_resize(int width, int height);

    // Check if engine wants to quit
    bool should_quit() const;

    // Cleanup
    void shutdown();

private:
    PlatformInterface* platform_;
    Camera camera_;
    std::unique_ptr<TileCache> cache_;
    std::unique_ptr<TileLoader> loader_;
    std::unique_ptr<Renderer> renderer_;

    bool quit_ = false;

    // Cached visible + prefetch tile list.
    // Recomputed only when camera is dirty, then reused by Renderer::render
    // so the render loop does zero heap allocation. See section 8.2.
    std::vector<TileId> visible_tiles_;
    std::vector<TileId> prefetch_tiles_;

    // Dataset metadata
    double ref_lat_, ref_lon_;  // ENU reference point
    float min_x_, max_x_, min_z_, max_z_;  // dataset bounds in world ENU

    void load_metadata(const std::string& dataset_name);
    void recompute_visible_tiles();
    void drain_pending_uploads();
};

} // namespace map_renderer
```

### 8.2 Update Flow

```
Engine::update(input_events, dt):
  1. For each input event:
       camera_.apply_input(event, dt)   // sets dirty_ if state changed
       if event.type == KeyQuit: quit_ = true
  2. If camera_.is_dirty():
       recompute_visible_tiles()        // fills visible_tiles_ + prefetch_tiles_
       loader_->request_tiles(prefetch_tiles_)   // visible + 1-ring prefetch
       // cancel tiles no longer in prefetch set:
       loader_->cancel_tiles(not_in(prefetch_tiles_))
       camera_.clear_dirty()
  3. drain_pending_uploads():
       newly = cache_->drain_recent_inserts()
       for id in newly:
           tile = cache_->get(id)
           if tile && !tile->uploaded:
               renderer_->on_tile_loaded(id, *tile)   // build geom + upload VBO
               tile->uploaded = true
               // FREE CPU feature data now that geometry is on the GPU:
               tile->buildings.clear(); tile->buildings.shrink_to_fit()
               tile->roads.clear();    tile->roads.shrink_to_fit()
               tile->polygons.clear(); tile->polygons.shrink_to_fit()
  4. renderer_->render(camera_, *cache_, visible_tiles_)
```

### 8.3 Visible + Prefetch Tile Computation (recompute_visible_tiles)

```
recompute_visible_tiles():
  z = camera_.get_tile_zoom()
  core = camera_.get_visible_tiles(z)          // tiles fully in viewport
  visible_tiles_ = core

  // Prefetch ring: expand the tile range by 1 in each direction so tiles
  // are already loaded (or loading) by the time the user pans into them.
  // This is the "prefetch ring" from FR-1.4.
  prefetch_tiles_ = expand_tile_range(core, +1)  // core + 1 border ring
```

`expand_tile_range` computes the bounding tile box of `core`, expands
min_x/min_y by 1 and max_x/max_y by 1, clamps to [0, 2^z), and emits all
TileIds in that box. The prefetch set is a superset of the visible set, so
requesting prefetch_tiles_ also requests everything visible.

### 8.4 CPU Memory Reclamation After GPU Upload

The renderer's `on_tile_loaded` builds geometry from the CPU feature
vectors and uploads it to a VBO. After that, the CPU vectors
(`buildings`, `roads`, `polygons`) are no longer needed — the GPU owns the
renderable geometry, and the draw ranges + world offset are already stored
on the `TileData`. The Engine frees them immediately after upload
(step 3 above) to keep resident memory low (HLD §8.2, FR-6.2). Only the
~40-byte TileData shell (id, offsets, ranges, GPU handles) stays cached.

---

## 9. Python Preprocessor

### 9.1 Interface (`tools/preprocess.py`)

```
Usage:
  python preprocess.py <input.pbf> <output_dir> [--zoom 8,12,15,17] [--ref-lat 22.0] [--ref-lon 78.0]

Example:
  python preprocess.py ~/Downloads/NewDelhi.osm.pbf data/tiles/new_delhi --zoom 8,12,15,17
  python preprocess.py ~/Downloads/india-260623.osm.pbf data/tiles/india --zoom 8,12,15,17
```

### 9.2 Processing Flow

```
1. Read PBF with osmium.FileReader
2. Compute dataset bounds (min/max lat/lon) from PBF header
3. Choose reference point (dataset center): ref_lat, ref_lon
4. For each zoom level z in [8, 12, 15, 17]:
   a. tolerance = zoom_tolerance[z]  (500, 50, 5, 0.5 meters)
   b. For each OSM object (node, way, relation):
      - Convert lat/lon to WORLD ENU meters using the EXACT formula:
            x = R * cos(radians(ref_lat)) * radians(lon - ref_lon)
            z = R * radians(lat - ref_lat)
        where R = 6371000.0. This MUST match the camera's inverse
        formula in §5.3 exactly.
      - Determine which tile(s) the feature intersects (world ENU
        → lat/lon → tile x/y via the inverse formula + slippy map math)
      - For each intersecting tile, compute the tile's center lat/lon
        (double precision) and re-express the feature's coordinates as
        PER-TILE local ENU meters relative to that tile center:
            local_x = world_x - tile_center_world_x
            local_z = world_z - tile_center_world_z
        (tile_center_world_x/z derived from tile center lat/lon via
         the forward formula). Store local_x/local_z as float32.
      - Skip buildings if z < 15
      - Apply Douglas-Peucker simplification to ways with tolerance
      - Serialize feature into Tile protobuf (with the tile's
        center_lat/lon in the header as double)
   c. For each non-empty tile:
      - Write zstd-compressed protobuf to output_dir/z/x/y.bin
5. Write metadata.bin (protobuf DatasetMetadata, zstd-compressed) to
   output_dir/. Contains: name, bounds, ref_lat/ref_lon, zoom_levels,
   total_tiles. NOT JSON — we have no JSON parser in the engine.
```

### 9.3 Tile File Path

```
output_dir/<z>/<x>/<y>.bin
```

Directory structure mirrors tile coordinates. No index file needed for v2.0 (the filesystem IS the index). A future version can pack tiles into a single indexed file with bbox queries.

### 9.4 Dependencies

```
# tools/requirements.txt
osmium==3.6.0
protobuf==4.25.1
zstandard==0.22.0
```

---

## 10. Debug Utilities

### 10.1 DEBUG_LOG (`engine/include/map_renderer/debug_log.h`)

```cpp
#pragma once

#ifdef MAP_RENDERER_DEBUG
    #include <cstdio>
    #define DEBUG_LOG(fmt, ...) \
        std::fprintf(stderr, "[map_renderer] " fmt "\n", ##__VA_ARGS__)
#else
    #define DEBUG_LOG(fmt, ...) ((void)0)
#endif
```

### 10.2 GL_CHECK (`engine/include/map_renderer/gl_check.h`)

```cpp
#pragma once

#include "debug_log.h"

#ifdef MAP_RENDERER_DEBUG
    // GL_CHECK takes a const GLFunctions& so it can be used in any scope,
    // not just Renderer methods. Uses standard C++ types (no GL headers
    // needed) — glGetError returns uint32_t, 0 means no error.
    #define GL_CHECK(gl) \
        do { \
            uint32_t err = (gl).glGetError(); \
            if (err != 0) { \
                DEBUG_LOG("GL error 0x%x at %s:%d", err, __FILE__, __LINE__); \
            } \
        } while (0)
#else
    #define GL_CHECK(gl) ((void)0)
#endif
```

Usage: `GL_CHECK(*gl_)` inside Renderer, `GL_CHECK(platform_.get_gl_functions())` elsewhere.

---

## 11. Testing

### 11.1 Unit Tests

| Module | Tests |
|--------|-------|
| Camera | Ortho matrix correctness, pan/zoom, dirty flag, tile zoom selection, visible tile range |
| GeometryBuilder | Ear-clipping (convex, concave, degenerate), road quad generation, winding order |
| TileLoader | Protobuf deserialization, zstd decompression, missing file handling |
| TileCache | LRU eviction, thread safety, capacity limits |
| ColorTable | Known colors, unknown type fallback |

### 11.2 Integration Tests

- Load a small test tile → build geometry → verify vertex counts and ranges
- Camera + tile cache: move camera → verify correct tiles requested
- Full pipeline: load metadata → frame dataset → render (headless with mock GL)

### 11.3 Test Infrastructure

- Google Test via CMake FetchContent
- Tests link against engine library
- Headless: no GL context needed for non-rendering tests (mock PlatformInterface)
- ASan/LSan: compile tests with -fsanitize=address,leak

---

## 12. Future Hooks (v3.0+)

These are NOT implemented in v2.0 but the architecture supports them:

### 12.1 3D Rendering
- `CameraMode::MODE_3D` already in the enum
- Building heights already in protobuf schema and internal structs
- Shader can be extended with a `u_mode` uniform to switch 2D/3D
- Geometry builder can add an extrude_buildings() method
- Depth buffer can be added to the render pass

### 12.2 JSON Styles
- Color table is a function returning `ColorEntry` — can be backed by a JSON-loaded map later
- No API change needed, just swap the implementation

### 12.3 Custom Indexed File Format
- TileLoader reads from filesystem — can be replaced with an indexed file reader
- TileId → file offset index, single file with all tiles
- Bbox query support for custom region extraction

### 12.4 Additional Zoom Levels
- Just preprocess more zoom levels — no code changes
- Camera zoom selection table can be updated or made dynamic

### 12.5 Network Tile Streaming
- TileLoader's file I/O can be replaced with HTTP requests
- Same cache, same LRU, same render path

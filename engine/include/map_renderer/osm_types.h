#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "tile_id.h"

namespace map_renderer {

struct Point {
    float x = 0.0f;
    float z = 0.0f;  // local ENU meters relative to tile center
};

struct Building {
    int64_t id = 0;
    std::vector<Point> footprint;
    float height = 0.0f;  // kept for future 3D
};

struct Road {
    int64_t id = 0;
    std::vector<Point> line;
    std::string type;
    float width = 6.0f;
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
    double center_lat = 0.0;
    double center_lon = 0.0;  // for computing world offset

    // CPU feature data (freed after GPU upload — see LLD §8.4)
    std::vector<Building> buildings;
    std::vector<Road> roads;
    std::vector<PolygonFeature> polygons;

    // Computed world offset (set by OSMLoader during deserialization)
    float world_offset_x = 0.0f;
    float world_offset_z = 0.0f;

    // GPU resources (owned by renderer, opaque handles)
    uint32_t vao = 0;
    uint32_t vbo = 0;

    // Vertex offsets within VBO (set by geometry builder)
    // offset/count are in VERTEX units (each vertex = 2 floats = 8 bytes)
    struct DrawRange {
        uint32_t offset = 0;
        uint32_t count = 0;
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

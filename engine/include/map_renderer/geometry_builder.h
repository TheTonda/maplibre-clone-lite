#pragma once

#include <cstdint>
#include <vector>

#include "map_renderer/osm_types.h"

namespace map_renderer {

// Output of GeometryBuilder::build_tile.
//
// All feature vertices are packed into a single flat VBO. Each vertex is
// two floats (x, z) = 8 bytes. DrawRange offset/count are in VERTEX units
// (not bytes), matching TileData::DrawRange (LLD §3.3, §4.1).
struct BuiltGeometry {
    // Flat vertex stream: x0, z0, x1, z1, ...
    std::vector<float> vertices;

    // Draw ranges (offset/count in vertices, not bytes).
    TileData::DrawRange water;
    TileData::DrawRange park;
    TileData::DrawRange landuse;
    TileData::DrawRange road;
    TileData::DrawRange building;
};

class GeometryBuilder {
public:
    // Build all geometry for a tile. Returns vertex data + draw ranges.
    // VBO order: water, park, landuse, road, building (LLD §6.3 draw order).
    static BuiltGeometry build_tile(const TileData& tile);

private:
    // Ear-clipping triangulation for simple polygons (no holes).
    // Returns triangle vertex indices into `polygon`.
    // Ensures CCW winding first; falls back to a fan on degenerate input.
    static std::vector<uint32_t> triangulate(const std::vector<Point>& polygon);

    // Generate road quads from a line string.
    // For each segment: compute perpendicular, extrude by width/2.
    // Output: 2 triangles per segment (6 vertices = 12 floats).
    static std::vector<float> build_road_quads(const std::vector<Point>& line,
                                               float width);

    // Triangulate a polygon and append its (x,z) vertices to out_vertices.
    static void append_triangulated_polygon(const std::vector<Point>& polygon,
                                            std::vector<float>& out_vertices);
};

} // namespace map_renderer

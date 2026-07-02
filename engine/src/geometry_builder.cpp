#include "map_renderer/geometry_builder.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace map_renderer {

namespace {

// Signed area via the shoelace formula (treating z as the y-axis in a
// standard right-handed x-y plane). Positive => counter-clockwise.
float signed_area(const std::vector<Point>& poly) {
    float area = 0.0f;
    const std::size_t n = poly.size();
    for (std::size_t i = 0; i < n; ++i) {
        const Point& a = poly[i];
        const Point& b = poly[(i + 1) % n];
        area += a.x * b.z - b.x * a.z;
    }
    return 0.5f * area;
}

// 2D cross product of vectors u and v.
float cross2d(float ux, float uz, float vx, float vz) {
    return ux * vz - uz * vx;
}

// True if point p is strictly inside CCW triangle (a, b, c).
// Points on an edge (a cross == 0) are NOT considered inside, which avoids
// falsely rejecting valid ears whose triangle edge touches another vertex.
bool point_in_triangle(const Point& p,
                       const Point& a, const Point& b, const Point& c) {
    const float d1 = cross2d(b.x - a.x, b.z - a.z, p.x - a.x, p.z - a.z);
    const float d2 = cross2d(c.x - b.x, c.z - b.z, p.x - b.x, p.z - b.z);
    const float d3 = cross2d(a.x - c.x, a.z - c.z, p.x - c.x, p.z - c.z);
    return d1 > 0.0f && d2 > 0.0f && d3 > 0.0f;
}

} // namespace

std::vector<uint32_t> GeometryBuilder::triangulate(const std::vector<Point>& polygon) {
    std::vector<uint32_t> indices;

    const std::size_t n = polygon.size();
    if (n < 3) {
        return indices; // nothing to triangulate
    }

    // Mutable list of vertex indices into `polygon`.
    std::vector<uint32_t> verts(n);
    for (std::size_t i = 0; i < n; ++i) {
        verts[i] = static_cast<uint32_t>(i);
    }

    // Ensure counter-clockwise winding (LLD §4.2 step 1).
    if (signed_area(polygon) < 0.0f) {
        std::reverse(verts.begin(), verts.end());
    }

    // Ear clipping (LLD §4.2 step 2). Each successful ear removes one
    // vertex, so the loop strictly terminates; the fan fallback breaks
    // out when no ear can be found (degenerate / collinear input).
    while (verts.size() > 3) {
        bool ear_found = false;
        const std::size_t m = verts.size();
        for (std::size_t i = 0; i < m; ++i) {
            const std::size_t prev_i = (i + m - 1) % m;
            const std::size_t next_i = (i + 1) % m;

            const Point& prev = polygon[verts[prev_i]];
            const Point& curr = polygon[verts[i]];
            const Point& next = polygon[verts[next_i]];

            // Convex check: cross of (curr - prev) and (next - curr) > 0.
            const float cross = cross2d(curr.x - prev.x, curr.z - prev.z,
                                        next.x - curr.x, next.z - curr.z);
            if (cross <= 0.0f) {
                continue; // reflex or collinear: not an ear
            }

            // No other vertex may be strictly inside triangle (prev, curr, next).
            bool contains_other = false;
            for (std::size_t j = 0; j < m; ++j) {
                if (j == prev_i || j == i || j == next_i) {
                    continue;
                }
                if (point_in_triangle(polygon[verts[j]], prev, curr, next)) {
                    contains_other = true;
                    break;
                }
            }
            if (contains_other) {
                continue;
            }

            // Ear: emit triangle (prev, curr, next) and remove curr.
            indices.push_back(verts[prev_i]);
            indices.push_back(verts[i]);
            indices.push_back(verts[next_i]);
            verts.erase(verts.begin() + static_cast<std::ptrdiff_t>(i));
            ear_found = true;
            break;
        }

        if (!ear_found) {
            // Degenerate fallback: fan triangulation from vertex 0 for all
            // remaining vertices (LLD §4.2 step 2b).
            for (std::size_t i = 1; i + 1 < verts.size(); ++i) {
                indices.push_back(verts[0]);
                indices.push_back(verts[i]);
                indices.push_back(verts[i + 1]);
            }
            verts.clear();
            break;
        }
    }

    // Emit final triangle (LLD §4.2 step 3). Skipped after the fan fallback
    // (verts is empty) or for the trivial 3-vertex polygon handled below.
    if (verts.size() == 3) {
        indices.push_back(verts[0]);
        indices.push_back(verts[1]);
        indices.push_back(verts[2]);
    }

    return indices;
}

std::vector<float> GeometryBuilder::build_road_quads(const std::vector<Point>& line,
                                                     float width) {
    std::vector<float> out;
    if (line.size() < 2 || width <= 0.0f) {
        return out;
    }

    const float half_w = width * 0.5f;
    for (std::size_t i = 0; i + 1 < line.size(); ++i) {
        const Point& p0 = line[i];
        const Point& p1 = line[i + 1];

        const float dx = p1.x - p0.x;
        const float dz = p1.z - p0.z;
        const float len = std::sqrt(dx * dx + dz * dz);
        if (len < 1e-6f) {
            continue; // degenerate segment
        }

        const float inv = 1.0f / len;
        const float dirx = dx * inv;
        const float dirz = dz * inv;
        // perp = (-dir.z, dir.x) (LLD §4.3)
        const float perpx = -dirz;
        const float perpz = dirx;

        const Point v0{p0.x + perpx * half_w, p0.z + perpz * half_w}; // left start
        const Point v1{p0.x - perpx * half_w, p0.z - perpz * half_w}; // right start
        const Point v2{p1.x + perpx * half_w, p1.z + perpz * half_w}; // left end
        const Point v3{p1.x - perpx * half_w, p1.z - perpz * half_w}; // right end

        // Triangle 1: (v0, v1, v2)
        out.push_back(v0.x); out.push_back(v0.z);
        out.push_back(v1.x); out.push_back(v1.z);
        out.push_back(v2.x); out.push_back(v2.z);
        // Triangle 2: (v1, v3, v2)
        out.push_back(v1.x); out.push_back(v1.z);
        out.push_back(v3.x); out.push_back(v3.z);
        out.push_back(v2.x); out.push_back(v2.z);
    }
    return out;
}

void GeometryBuilder::append_triangulated_polygon(const std::vector<Point>& polygon,
                                                  std::vector<float>& out_vertices) {
    const std::vector<uint32_t> indices = triangulate(polygon);
    for (const uint32_t idx : indices) {
        out_vertices.push_back(polygon[idx].x);
        out_vertices.push_back(polygon[idx].z);
    }
}

BuiltGeometry GeometryBuilder::build_tile(const TileData& tile) {
    BuiltGeometry geom;

    // Helper: record the vertex count (vertices = floats / 2) before/after
    // appending a feature group into a DrawRange (units = vertices).
    const auto vertex_count = [&geom]() {
        return geom.vertices.size() / 2;
    };

    // VBO order: water, park, landuse, road, building (LLD §6.3).

    // Water polygons
    std::size_t before = vertex_count();
    for (const auto& poly : tile.polygons) {
        if (poly.type == "water") {
            append_triangulated_polygon(poly.polygon, geom.vertices);
        }
    }
    geom.water.offset = static_cast<uint32_t>(before);
    geom.water.count = static_cast<uint32_t>(vertex_count() - before);

    // Park polygons
    before = vertex_count();
    for (const auto& poly : tile.polygons) {
        if (poly.type == "park") {
            append_triangulated_polygon(poly.polygon, geom.vertices);
        }
    }
    geom.park.offset = static_cast<uint32_t>(before);
    geom.park.count = static_cast<uint32_t>(vertex_count() - before);

    // Landuse polygons
    before = vertex_count();
    for (const auto& poly : tile.polygons) {
        if (poly.type == "landuse") {
            append_triangulated_polygon(poly.polygon, geom.vertices);
        }
    }
    geom.landuse.offset = static_cast<uint32_t>(before);
    geom.landuse.count = static_cast<uint32_t>(vertex_count() - before);

    // Roads
    before = vertex_count();
    for (const auto& road : tile.roads) {
        const std::vector<float> quads = build_road_quads(road.line, road.width);
        geom.vertices.insert(geom.vertices.end(), quads.begin(), quads.end());
    }
    geom.road.offset = static_cast<uint32_t>(before);
    geom.road.count = static_cast<uint32_t>(vertex_count() - before);

    // Buildings (triangulated footprints)
    before = vertex_count();
    for (const auto& building : tile.buildings) {
        append_triangulated_polygon(building.footprint, geom.vertices);
    }
    geom.building.offset = static_cast<uint32_t>(before);
    geom.building.count = static_cast<uint32_t>(vertex_count() - before);

    return geom;
}

} // namespace map_renderer

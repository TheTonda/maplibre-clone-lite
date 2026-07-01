/// @file geometry_builder.cpp
/// @brief Converts OSM data into GPU-vertex/index buffers.

#include "data/geometry_builder.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include <glm/glm.hpp>

// ======================================================================
// 2D helpers (anonymous namespace for internal linkage)
// ======================================================================

namespace {

/// Cross product of vectors ab × ac in the x–z plane.
float cross_2d(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

/// Check if point p is inside triangle (a,b,c).  Barycentric sign test.
bool point_in_triangle(const glm::vec2& p,
                       const glm::vec2& a,
                       const glm::vec2& b,
                       const glm::vec2& c)
{
    float d1 = cross_2d(p, a, b);
    float d2 = cross_2d(p, b, c);
    float d3 = cross_2d(p, c, a);
    bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(has_neg && has_pos);
}

/// Check if vertex at index i in the active polygon is an ear.
bool is_ear(const std::vector<glm::vec2>& verts,
            const std::vector<int>& indices,
            size_t i)
{
    int n = static_cast<int>(indices.size());
    int prev = indices[(i - 1 + n) % n];
    int curr = indices[i];
    int next = indices[(i + 1) % n];

    if (cross_2d(verts[prev], verts[curr], verts[next]) <= 0.0f)
        return false;

    for (int j = 0; j < n; ++j) {
        int idx = indices[j];
        if (idx == prev || idx == curr || idx == next)
            continue;
        if (point_in_triangle(verts[idx], verts[prev], verts[curr], verts[next]))
            return false;
    }
    return true;
}

} // anonymous namespace

std::vector<uint32_t> GeometryBuilder::triangulate_polygon(
    const std::vector<osm::Point>& polygon,
    std::vector<glm::vec2>& out_vertices)
{
    std::vector<uint32_t> tris;
    if (polygon.size() < 3) return tris;

    // Build index list
    std::vector<int> indices;
    indices.reserve(polygon.size());
    for (size_t i = 0; i < polygon.size(); ++i)
        indices.push_back(static_cast<int>(i));

    // Copy vertices to output, tracking base offset
    uint32_t base = static_cast<uint32_t>(out_vertices.size());
    for (const auto& p : polygon)
        out_vertices.push_back({p.x, p.z});

    int n = static_cast<int>(indices.size());

    // Ear-clip loop
    while (n > 3) {
        bool ear_found = false;
        for (int i = 0; i < n; ++i) {
            if (is_ear(out_vertices, indices, static_cast<size_t>(i))) {
                int prv = indices[(i - 1 + n) % n];
                int cur = indices[i];
                int nxt = indices[(i + 1) % n];
                tris.push_back(base + prv);
                tris.push_back(base + cur);
                tris.push_back(base + nxt);

                indices.erase(indices.begin() + i);
                --n;
                ear_found = true;
                break;
            }
        }
        // Fallback: fan from first vertex if no ear found (degenerate)
        if (!ear_found) {
            for (int j = 1; j < n - 1; ++j) {
                tris.push_back(base + indices[0]);
                tris.push_back(base + indices[j]);
                tris.push_back(base + indices[j + 1]);
            }
            break;
        }
    }

    // Final triangle
    if (n == 3) {
        tris.push_back(base + indices[0]);
        tris.push_back(base + indices[1]);
        tris.push_back(base + indices[2]);
    }

    return tris;
}

// ======================================================================
// Building extrusion
// ======================================================================

BuildingMesh GeometryBuilder::build_building(const osm::Building& building) {
    BuildingMesh mesh;
    if (building.footprint.size() < 3) return mesh;

    const float h = building.height_m;
    auto& verts = mesh.vertices;
    auto& idxs  = mesh.indices;

    // --- Triangulate footprint ---
    std::vector<glm::vec2> cap_verts;
    auto cap_tris = triangulate_polygon(building.footprint, cap_verts);

    // Bottom cap (y = 0), flipped winding
    uint32_t bot = static_cast<uint32_t>(verts.size());
    for (const auto& v : cap_verts)
        verts.push_back({{v.x, 0.0f, v.y}, {0.0f, -1.0f, 0.0f}});
    for (int i = static_cast<int>(cap_tris.size()) - 3; i >= 0; i -= 3) {
        idxs.push_back(bot + cap_tris[i + 2]);
        idxs.push_back(bot + cap_tris[i + 1]);
        idxs.push_back(bot + cap_tris[i]);
    }

    // Top cap (y = h), normal up
    uint32_t top = static_cast<uint32_t>(verts.size());
    for (const auto& v : cap_verts)
        verts.push_back({{v.x, h, v.y}, {0.0f, 1.0f, 0.0f}});
    for (auto idx : cap_tris)
        idxs.push_back(top + idx);

    // --- Side walls (one quad per edge) ---
    const auto& fp = building.footprint;
    size_t n = fp.size();
    for (size_t i = 0; i < n; ++i) {
        size_t nxt = (i + 1) % n;
        glm::vec2 a(fp[i].x,   fp[i].z);
        glm::vec2 b(fp[nxt].x, fp[nxt].z);
        glm::vec2 edge = b - a;
        glm::vec3 nrm = glm::normalize(glm::vec3(edge.y, 0.0f, -edge.x));

        uint32_t i0 = static_cast<uint32_t>(verts.size());
        verts.push_back({{a.x, 0.0f, a.y}, nrm});
        verts.push_back({{a.x, h,    a.y}, nrm});
        verts.push_back({{b.x, 0.0f, b.y}, nrm});
        verts.push_back({{b.x, h,    b.y}, nrm});

        idxs.push_back(i0); idxs.push_back(i0 + 1); idxs.push_back(i0 + 2);
        idxs.push_back(i0 + 1); idxs.push_back(i0 + 3); idxs.push_back(i0 + 2);
    }

    return mesh;
}

// ======================================================================
// Road quads
// ======================================================================

LineMesh GeometryBuilder::build_road_quad(const osm::Road& road) {
    LineMesh mesh;
    if (road.line.size() < 2) return mesh;

    const float half_w = road.width_m / 2.0f;
    auto& verts = mesh.vertices;
    auto& idxs  = mesh.indices;

    for (size_t i = 0; i < road.line.size() - 1; ++i) {
        glm::vec2 a(road.line[i].x,     road.line[i].z);
        glm::vec2 b(road.line[i + 1].x, road.line[i + 1].z);
        glm::vec2 dir = b - a;
        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (len < 0.001f) continue;
        dir /= len;

        glm::vec2 perp(-dir.y * half_w, dir.x * half_w);

        uint32_t base = static_cast<uint32_t>(verts.size());
        verts.push_back({{a.x - perp.x, 0.0f, a.y - perp.y}});
        verts.push_back({{a.x + perp.x, 0.0f, a.y + perp.y}});
        verts.push_back({{b.x - perp.x, 0.0f, b.y - perp.y}});
        verts.push_back({{b.x + perp.x, 0.0f, b.y + perp.y}});

        idxs.push_back(base); idxs.push_back(base + 2); idxs.push_back(base + 1);
        idxs.push_back(base + 1); idxs.push_back(base + 2); idxs.push_back(base + 3);
    }

    return mesh;
}

// ======================================================================
// Ground plane
// ======================================================================

GroundMesh GeometryBuilder::build_ground(float half_size) {
    GroundMesh mesh;
    mesh.vertices = {
        {{-half_size, -half_size}},
        {{ half_size, -half_size}},
        {{ half_size,  half_size}},
        {{-half_size,  half_size}},
    };
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

// ======================================================================
// Polygon fill helper (re-uses ear-clipping)
// ======================================================================

static PolygonMesh build_polygon_mesh(
    const std::vector<osm::PolygonFeature>& features)
{
    PolygonMesh mesh;
    for (const auto& f : features) {
        std::vector<glm::vec2> tmp;
        auto tris = GeometryBuilder::triangulate_polygon(f.polygon, tmp);
        // Append new vertices
        uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
        for (const auto& t : tmp)
            mesh.vertices.push_back(FillVertex{{t.x, t.y}});
        // Re-base index offsets
        for (auto idx : tris)
            mesh.indices.push_back(base + idx);
    }
    return mesh;
}

// ======================================================================
// Bulk builder
// ======================================================================

GeometryData GeometryBuilder::build_all(const osm::OSMData& data,
                                        float half_size)
{
    GeometryData g;

    // Ground
    g.ground = build_ground(half_size);

    // Buildings
    g.buildings.vertices.reserve(data.buildings.size() * 20);
    g.buildings.indices.reserve(data.buildings.size() * 60);
    for (const auto& b : data.buildings) {
        auto bm = build_building(b);
        g.buildings.vertices.insert(g.buildings.vertices.end(),
                                     bm.vertices.begin(), bm.vertices.end());
        g.buildings.indices.insert(g.buildings.indices.end(),
                                    bm.indices.begin(), bm.indices.end());
    }

    // Polygon features
    g.parks   = build_polygon_mesh(data.parks);
    g.water   = build_polygon_mesh(data.water);
    g.landuse = build_polygon_mesh(data.landuse);

    // Roads – separate by type for independent styling
    auto append_road = [](LineMesh& dst, const LineMesh& src) {
        dst.vertices.insert(dst.vertices.end(),
                            src.vertices.begin(), src.vertices.end());
        dst.indices.insert(dst.indices.end(),
                           src.indices.begin(), src.indices.end());
    };

    for (const auto& r : data.roads) {
        auto quad = build_road_quad(r);
        if (r.type == "primary" || r.type == "motorway" || r.type == "trunk") {
            append_road(g.roads_primary, quad);
        } else if (r.type == "secondary") {
            append_road(g.roads_secondary, quad);
        } else if (r.type == "residential" || r.type == "tertiary") {
            append_road(g.roads_residential, quad);
        } else {
            append_road(g.roads_service, quad);
        }
    }

    return g;
}

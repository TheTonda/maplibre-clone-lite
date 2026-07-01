#pragma once
// building_data.h — Converts 2D building footprints into 3D extruded box geometry
// Each building footprint polygon is extruded upward by its height to form:
//   - Top face (flat roof)
//   - 4 side faces (one per edge of the polygon)
// Output: vertex + index buffers ready for VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST

#include "osm_loader.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include <glm/glm.hpp>

#ifdef MAP_RENDERER_DEBUG
#define DEBUG_LOG(...) std::printf("[DEBUG] " __VA_ARGS__); std::printf("\n")
#else
#define DEBUG_LOG(...) ((void)0)
#endif

namespace bldg {

// ─── Vertex format for 3D building geometry ────────────────────────────

struct BuildingVertex {
    float x, y, z;  // world-space position
};

// ─── A batch of extruded building geometry ready for GPU ───────────────

struct BuildingBatch {
    std::vector<BuildingVertex> vertices;
    std::vector<uint32_t> indices;
};

// ─── Convert a 2D footprint ring to 3D extruded box ───────────────────

/// Extrude a 2D polygon footprint (in Mercator world coords) into a 3D box.
/// The polygon is treated as a flat ring in the XZ plane (Y is up).
/// Produces: top face (flat), bottom face (flat), and side faces (one per edge).
inline void extrude_building(
    const std::vector<osm::MercatorPoint>& footprint,
    float height,
    std::vector<BuildingVertex>& vertices,
    std::vector<uint32_t>& indices)
{
    if (footprint.size() < 3) return;

    DEBUG_LOG("extrude_building: footprint=%zu points, height=%.1f",
              footprint.size(), height);

    const float y_top = height;
    const float y_bot = 0.0f;

    uint32_t base = static_cast<uint32_t>(vertices.size());
    uint32_t n = static_cast<uint32_t>(footprint.size());

    // Expand the ring: for each edge, add a slight offset to close gaps
    // between adjacent buildings (avoids z-fighting)
    constexpr float GAP = 0.5f;

    // Collect top and bottom vertices
    for (uint32_t i = 0; i < n; ++i) {
        auto& p = footprint[i];
        vertices.push_back({p.x, y_top, p.y});   // top
        vertices.push_back({p.x, y_bot, p.y});   // bottom
    }

    // Top face: fan triangulation from first top vertex
    for (uint32_t i = 1; i + 1 < n; ++i) {
        indices.push_back(base);
        indices.push_back(base + 2 * i);
        indices.push_back(base + 2 * (i + 1));
    }

    // Bottom face: fan triangulation (reversed winding for outward normal)
    uint32_t bottom_base = base + n;
    for (uint32_t i = 1; i + 1 < n; ++i) {
        indices.push_back(bottom_base);
        indices.push_back(bottom_base + 2 * (i + 1));
        indices.push_back(bottom_base + 2 * i);
    }

    // Side faces: one quad per edge (two triangles)
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t next = (i + 1) % n;
        uint32_t t0 = base + 2 * i;       // top-front
        uint32_t t1 = base + 2 * next;    // top-back
        uint32_t b0 = base + 2 * i + 1;   // bot-front
        uint32_t b1 = base + 2 * next + 1; // bot-back

        // Front face (CW when viewed from outside)
        indices.push_back(t0);
        indices.push_back(t1);
        indices.push_back(b1);

        indices.push_back(t0);
        indices.push_back(b1);
        indices.push_back(b0);
    }
}

// ─── Convert all loaded buildings to GPU-ready batches ─────────────────

/// Extract building geometry from loaded OSM data.
/// Coordinates are normalized so the data center is at (0, 0).
/// Returns a single BuildingBatch with all buildings merged.
inline BuildingBatch extract_buildings(const std::vector<osm::Building>& buildings) {
    BuildingBatch batch;

    // First pass: compute bounding box of all building footprints
    float min_x = 1e9f, min_y = 1e9f;
    float max_x = -1e9f, max_y = -1e9f;
    for (const auto& b : buildings) {
        for (const auto& p : b.footprint) {
            if (p.x < min_x) min_x = p.x;
            if (p.y < min_y) min_y = p.y;
            if (p.x > max_x) max_x = p.x;
            if (p.y > max_y) max_y = p.y;
        }
    }

    // Compute center for normalization
    float center_x = (min_x + max_x) * 0.5f;
    float center_y = (min_y + max_y) * 0.5f;

    DEBUG_LOG("Building data center: (%.1f, %.1f)", center_x, center_y);
    DEBUG_LOG("Building data range: X(%.0f to %.0f) Y(%.0f to %.0f)",
              min_x, max_x, min_y, max_y);

    // Second pass: extract buildings with normalized coordinates
    for (const auto& b : buildings) {
        if (b.footprint.size() < 3) continue;

        // Normalize footprint to center origin
        std::vector<osm::MercatorPoint> normalized_footprint;
        normalized_footprint.reserve(b.footprint.size());
        for (const auto& p : b.footprint) {
            normalized_footprint.push_back({p.x - center_x, p.y - center_y});
        }

        DEBUG_LOG("  extract: id=%lld, footprint=%zu, height=%.1f",
                  (long long)b.id, b.footprint.size(), b.height);
        extrude_building(normalized_footprint, b.height, batch.vertices, batch.indices);
    }

    printf("building_data: %zu vertices, %zu indices across %zu buildings (normalized to origin)\n",
           batch.vertices.size(), batch.indices.size(), buildings.size());

    return batch;
}

// ─── Convert parks/water/landuse to 2D fill geometry ──────────────────

/// Convert a collection of polygon features into a 2D fill vertex/index batch.
/// Uses fan triangulation for each polygon.
inline std::vector<std::pair<std::vector<osm::MercatorPoint>, glm::vec3>>
extract_fills_2d(const std::vector<osm::Park>& parks,
                 const std::vector<osm::WaterPolygon>& water,
                 const std::vector<osm::Landuse>& landuse,
                 glm::vec3 park_color,
                 glm::vec3 water_color,
                 glm::vec3 land_color)
{
    std::vector<std::pair<std::vector<osm::MercatorPoint>, glm::vec3>> result;

    for (const auto& p : parks) {
        if (p.polygon.size() >= 3) {
            result.push_back({p.polygon, park_color});
        }
    }

    for (const auto& w : water) {
        if (w.polygon.size() >= 3) {
            result.push_back({w.polygon, water_color});
        }
    }

    for (const auto& l : landuse) {
        if (l.polygon.size() >= 3) {
            result.push_back({l.polygon, land_color});
        }
    }

    printf("extract_fills_2d: %zu polygon groups\n", result.size());
    return result;
}

} // namespace bldg

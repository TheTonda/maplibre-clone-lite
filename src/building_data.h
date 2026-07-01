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
    float x, y, z;      // world-space position
    float nx, ny, nz;   // surface normal
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
/// Each face has its own vertices with proper normals for correct lighting.
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

    uint32_t n = static_cast<uint32_t>(footprint.size());

    // ─── Top face (normal = 0, 1, 0) ────────────────────────────────
    uint32_t top_base = static_cast<uint32_t>(vertices.size());
    for (uint32_t i = 0; i < n; ++i) {
        const auto& p = footprint[i];
        vertices.push_back({p.x, y_top, p.y, 0.0f, 1.0f, 0.0f});
    }
    // Fan triangulation (CCW when viewed from above)
    for (uint32_t i = 1; i + 1 < n; ++i) {
        indices.push_back(top_base);
        indices.push_back(top_base + i);
        indices.push_back(top_base + i + 1);
    }

    // ─── Bottom face (normal = 0, -1, 0) ────────────────────────────
    uint32_t bottom_base = static_cast<uint32_t>(vertices.size());
    for (uint32_t i = 0; i < n; ++i) {
        const auto& p = footprint[i];
        vertices.push_back({p.x, y_bot, p.y, 0.0f, -1.0f, 0.0f});
    }
    // Fan triangulation (CW when viewed from above = CCW from below)
    for (uint32_t i = 1; i + 1 < n; ++i) {
        indices.push_back(bottom_base);
        indices.push_back(bottom_base + i + 1);
        indices.push_back(bottom_base + i);
    }

    // ─── Side faces (one per edge) ──────────────────────────────────
    // Each side face has its own 4 vertices with normal perpendicular to the edge
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t next = (i + 1) % n;
        const auto& p0 = footprint[i];
        const auto& p1 = footprint[next];

        // Edge vector in XZ plane
        float ex = p1.x - p0.x;
        float ez = p1.y - p0.y;  // footprint.y is Z in world space

        // Outward normal (perpendicular to edge, pointing away from polygon center)
        // For CCW polygon (standard), normal = (-ez, 0, ex) normalized
        // But we need to check winding - for now assume CCW
        float len = std::sqrt(ex * ex + ez * ez);
        if (len < 1e-6f) continue;  // Degenerate edge

        float nx = -ez / len;
        float nz = ex / len;

        // Four corners of this side face
        uint32_t side_base = static_cast<uint32_t>(vertices.size());
        // Top-front (p0 at top)
        vertices.push_back({p0.x, y_top, p0.y, nx, 0.0f, nz});
        // Top-back (p1 at top)
        vertices.push_back({p1.x, y_top, p1.y, nx, 0.0f, nz});
        // Bottom-back (p1 at bottom)
        vertices.push_back({p1.x, y_bot, p1.y, nx, 0.0f, nz});
        // Bottom-front (p0 at bottom)
        vertices.push_back({p0.x, y_bot, p0.y, nx, 0.0f, nz});

        // Two triangles forming the quad
        // CCW when viewed from outside (from the normal direction)
        indices.push_back(side_base + 0);  // top-front
        indices.push_back(side_base + 1);  // top-back
        indices.push_back(side_base + 2);  // bot-back

        indices.push_back(side_base + 0);  // top-front
        indices.push_back(side_base + 2);  // bot-back
        indices.push_back(side_base + 3);  // bot-front
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

// ─── Convert 2D features to 3D ground geometry ────────────────────────

struct GroundFeatureBatch {
    std::vector<float> vertices;  // x, z pairs (flat)
    std::vector<uint32_t> indices;
    glm::vec3 color;
};

/// Convert parks/water/landuse to 3D ground geometry (triangulated, normalized to origin)
/// Coordinates are normalized so data center is at (0, 0), and Y is set to 0.1
/// to avoid z-fighting with the ground plane.
inline std::vector<GroundFeatureBatch>
extract_ground_features(const std::vector<osm::Park>& parks,
                        const std::vector<osm::WaterPolygon>& water,
                        const std::vector<osm::Landuse>& landuse,
                        glm::vec3 park_color,
                        glm::vec3 water_color,
                        glm::vec3 land_color)
{
    // First pass: find data center
    float min_x = 1e9f, min_y = 1e9f;
    float max_x = -1e9f, max_y = -1e9f;
    for (const auto& p : parks) {
        for (const auto& pt : p.polygon) {
            if (pt.x < min_x) min_x = pt.x;
            if (pt.y < min_y) min_y = pt.y;
            if (pt.x > max_x) max_x = pt.x;
            if (pt.y > max_y) max_y = pt.y;
        }
    }
    for (const auto& w : water) {
        for (const auto& pt : w.polygon) {
            if (pt.x < min_x) min_x = pt.x;
            if (pt.y < min_y) min_y = pt.y;
            if (pt.x > max_x) max_x = pt.x;
            if (pt.y > max_y) max_y = pt.y;
        }
    }
    for (const auto& l : landuse) {
        for (const auto& pt : l.polygon) {
            if (pt.x < min_x) min_x = pt.x;
            if (pt.y < min_y) min_y = pt.y;
            if (pt.x > max_x) max_x = pt.x;
            if (pt.y > max_y) max_y = pt.y;
        }
    }
    float center_x = (min_x + max_x) * 0.5f;
    float center_y = (min_y + max_y) * 0.5f;

    std::vector<GroundFeatureBatch> result;

    // Process each type separately (so each can have its own color)
    auto process_polygons = [&](const auto& polygons, glm::vec3 color) {
        GroundFeatureBatch batch;
        batch.color = color;
        for (const auto& feature : polygons) {
            if (feature.polygon.size() < 3) continue;
            uint32_t base = static_cast<uint32_t>(batch.vertices.size() / 2);
            for (const auto& pt : feature.polygon) {
                batch.vertices.push_back(pt.x - center_x);
                batch.vertices.push_back(pt.y - center_y);
            }
            // Fan triangulation
            for (uint32_t i = 1; i + 1 < feature.polygon.size(); ++i) {
                batch.indices.push_back(base);
                batch.indices.push_back(base + i);
                batch.indices.push_back(base + i + 1);
            }
        }
        if (!batch.vertices.empty()) {
            result.push_back(std::move(batch));
        }
    };

    process_polygons(parks, park_color);
    process_polygons(water, water_color);
    process_polygons(landuse, land_color);

    printf("extract_ground_features: %zu feature groups (normalized to origin)\n", result.size());
    return result;
}

// ─── Convert roads to 3D line geometry ─────────────────────────────────

struct RoadLineBatch {
    std::vector<float> vertices;  // x, z pairs (flat)
    std::vector<uint32_t> indices;
};

/// Convert roads to 3D line geometry (normalized to origin)
inline RoadLineBatch
extract_road_lines(const std::vector<osm::Road>& roads)
{
    // Find data center
    float min_x = 1e9f, min_y = 1e9f;
    float max_x = -1e9f, max_y = -1e9f;
    for (const auto& r : roads) {
        for (const auto& pt : r.line) {
            if (pt.x < min_x) min_x = pt.x;
            if (pt.y < min_y) min_y = pt.y;
            if (pt.x > max_x) max_x = pt.x;
            if (pt.y > max_y) max_y = pt.y;
        }
    }
    float center_x = (min_x + max_x) * 0.5f;
    float center_y = (min_y + max_y) * 0.5f;

    RoadLineBatch batch;
    for (const auto& road : roads) {
        if (road.line.size() < 2) continue;
        uint32_t base = static_cast<uint32_t>(batch.vertices.size() / 2);
        for (const auto& pt : road.line) {
            batch.vertices.push_back(pt.x - center_x);
            batch.vertices.push_back(pt.y - center_y);
        }
        // Line strip with primitive restart (use UINT32_MAX as restart)
        for (uint32_t i = 0; i < road.line.size(); ++i) {
            batch.indices.push_back(base + i);
            if (i < road.line.size() - 1) {
                // Add a small gap marker (not used in simple line rendering)
            }
        }
        // Add a restart index to separate roads
        if (base > 0) {
            batch.indices.push_back(0xFFFFFFFF);
        }
    }

    printf("extract_road_lines: %zu vertices, %zu indices\n",
           batch.vertices.size() / 2, batch.indices.size());
    return batch;
}

} // namespace bldg

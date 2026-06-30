#pragma once
// render_data.h — Converts MVT parsed geometry into Vulkan-ready vertex/index buffers
// Decodes LineString and Polygon features from a tile, scales tile coords → clip
// space (-1..1), and produces interleaved vertex + index arrays.
// Lines use VK_PRIMITIVE_TOPOLOGY_LINE_STRIP with primitive restart (0xFFFFFFFF).
// Polygons use fan triangulation for VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST.

#include "mvt_parser.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace render {

// ─── Vertex format (shared by lines and polygons) ──────────────────

struct LineVertex {
    float x, y;
};

// ─── A batch of line geometry ready for GPU upload ─────────────────

struct LineBatch {
    std::vector<LineVertex>  vertices;  // interleaved x,y positions (clip space)
    std::vector<uint32_t>    indices;   // indexed draw list with 0xFFFFFFFF restart
};

// ─── Polygon batches ───────────────────────────────────────────────

struct PolyVertex {
    float x, y;
};

struct PolyBatch {
    std::vector<PolyVertex> vertices;   // interleaved x,y positions (clip space)
    std::vector<uint32_t>   indices;    // triangle-list indices (3 per triangle)
    std::string             layer_name; // source layer for style matching
};

// ─── Internal: a single decoded line segment ───────────────────────

struct LineSegment {
    std::vector<std::pair<int32_t, int32_t>> points;  // tile-local coords
    std::string layer_name;  // source layer for style matching
};

// ─── LineString geometry decoder ───────────────────────────────────

/// Decode an MVT feature geometry stream into one or more LineSegments.
/// Each MoveTo starts a new segment; LineTo continues it.
/// Coordinates remain in tile-local space (0..extent).
inline std::vector<LineSegment> decode_linestring_geometry(
    const std::vector<uint32_t>& geometry,
    uint32_t /*extent*/)
{
    std::vector<LineSegment> segments;
    LineSegment current;
    int32_t cx = 0, cy = 0;   // cursor
    bool first_move = true;

    size_t i = 0;
    while (i < geometry.size()) {
        uint32_t cmd = geometry[i++];
        int cmd_id = cmd & 0x7;
        int count  = static_cast<int>(cmd >> 3);

        if (cmd_id == 1) {   // MoveTo
            // Flush previous segment if it has points
            if (!current.points.empty()) {
                segments.push_back(std::move(current));
                current = LineSegment{};
            }
            for (int j = 0; j < count && i + 1 < geometry.size(); ++j) {
                int32_t dx = mvt::zigzag_decode(geometry[i++]);
                int32_t dy = mvt::zigzag_decode(geometry[i++]);
                if (first_move && j == 0) {
                    // Very first point of the geometry — use as-is (cursor was 0,0)
                    cx = dx;
                    cy = dy;
                    first_move = false;
                } else {
                    cx += dx;
                    cy += dy;
                }
                current.points.push_back({cx, cy});
            }
        } else if (cmd_id == 2) {   // LineTo
            for (int j = 0; j < count && i + 1 < geometry.size(); ++j) {
                int32_t dx = mvt::zigzag_decode(geometry[i++]);
                int32_t dy = mvt::zigzag_decode(geometry[i++]);
                cx += dx;
                cy += dy;
                current.points.push_back({cx, cy});
            }
        } else if (cmd_id == 7) {   // ClosePath — connect back to first point
            if (!current.points.empty()) {
                current.points.push_back(current.points[0]);
            }
            // ClosePath has no coordinate parameters (count is typically 1)
        } else {
            // Unknown command — skip its parameters
            i += static_cast<size_t>(count) * 2;
        }
    }

    if (!current.points.empty()) {
        segments.push_back(std::move(current));
    }

    return segments;
}

// ─── Tile → GPU-ready batch extraction ─────────────────────────────

/// Extract all LineString features from an MVT tile and produce
/// a combined vertex+index buffer with positions in clip space.
///
/// If `layer_filter` is non-empty, only features from layers whose name
/// contains the filter string are included.
inline LineBatch extract_lines(const mvt::Tile& tile,
                               const std::string& layer_filter = "")
{
    LineBatch batch;

    for (const auto& layer : tile.layers) {
        // Apply optional layer name filter
        if (!layer_filter.empty() &&
            layer.name.find(layer_filter) == std::string::npos) {
            continue;
        }

        for (const auto& feat : layer.features) {
            if (feat.type != mvt::GeomType::LINESTRING) continue;
            if (feat.geometry.empty()) continue;

            auto segments = decode_linestring_geometry(feat.geometry, layer.extent);

            for (auto& seg : segments) {
                if (seg.points.size() < 2) continue;  // need at least 2 points for a line

                seg.layer_name = layer.name;

                // Append vertices (scale tile coords → clip space)
                uint32_t base_idx = static_cast<uint32_t>(batch.vertices.size());

                float inv_extent = 1.0f / static_cast<float>(layer.extent);
                for (auto [x, y] : seg.points) {
                    // X:  0..extent  →  -1..1
                    // Y:  0..extent  →   1..-1  (Vulkan's Y-down convention)
                    float clip_x = (static_cast<float>(x) * inv_extent) * 2.0f - 1.0f;
                    float clip_y = 1.0f - (static_cast<float>(y) * inv_extent) * 2.0f;
                    batch.vertices.push_back({clip_x, clip_y});
                }

                // Build index list: simple sequential indices for this strip
                uint32_t n = static_cast<uint32_t>(seg.points.size());
                for (uint32_t k = 0; k < n; ++k) {
                    batch.indices.push_back(base_idx + k);
                }
                // Primitive restart separates this strip from the next
                batch.indices.push_back(0xFFFFFFFF);
            }
        }
    }

    return batch;
}

// ─── Fan triangulation helper ─────────────────────────────────────

/// Fan-triangulate a single ring (no holes). First vertex is the hub;
/// for each subsequent pair (v1, v2), emit triangle (hub, v1, v2).
inline void fan_triangulate(
    const std::vector<std::pair<int32_t, int32_t>>& ring,
    float inv_extent,
    std::vector<PolyVertex>& vertices,
    std::vector<uint32_t>& indices)
{
    if (ring.size() < 3) return;

    uint32_t base = static_cast<uint32_t>(vertices.size());

    // Convert ring points to clip-space vertices
    for (auto [x, y] : ring) {
        float clip_x = (static_cast<float>(x) * inv_extent) * 2.0f - 1.0f;
        float clip_y = 1.0f - (static_cast<float>(y) * inv_extent) * 2.0f;
        vertices.push_back({clip_x, clip_y});
    }

    // Emit fan triangles: (base, base+i, base+i+1) for i=1..n-2
    uint32_t n = static_cast<uint32_t>(ring.size());
    for (uint32_t i = 1; i + 1 < n; ++i) {
        indices.push_back(base);
        indices.push_back(base + i);
        indices.push_back(base + i + 1);
    }
}

// ─── Polygon geometry decoder ─────────────────────────────────────

/// Decode an MVT polygon feature into a list of rings.
/// Each MoveTo starts a new ring; LineTo continues it; ClosePath
/// connects back to the first point of the ring.
/// Coordinates remain in tile-local space (0..extent).
inline std::vector<LineSegment> decode_polygon_rings(
    const std::vector<uint32_t>& geometry,
    uint32_t /*extent*/)
{
    // Reuse the same decode logic — polygon geometry uses the same
    // command stream format (MoveTo, LineTo, ClosePath).
    return decode_linestring_geometry(geometry, 0);
}

// ─── Tile → GPU-ready polygon batch extraction ────────────────────

/// Extract all Polygon features from an MVT tile and produce
/// a combined vertex+index buffer with fan-triangulated positions
/// in clip space.
///
/// If `layer_filter` is non-empty, only features from layers whose name
/// contains the filter string are included.
///
/// For polygon features with multiple rings (holes), only the first
/// (outer) ring is triangulated.
inline PolyBatch extract_polygons(const mvt::Tile& tile,
                                  const std::string& layer_filter = "")
{
    PolyBatch batch;

    for (const auto& layer : tile.layers) {
        // Apply optional layer name filter
        if (!layer_filter.empty() &&
            layer.name.find(layer_filter) == std::string::npos) {
            continue;
        }

        for (const auto& feat : layer.features) {
            if (feat.type != mvt::GeomType::POLYGON) continue;
            if (feat.geometry.empty()) continue;

            auto rings = decode_polygon_rings(feat.geometry, layer.extent);
            if (rings.empty()) continue;

            // Use the first (outer) ring only — skip holes for now
            auto& outer_ring = rings[0];
            if (outer_ring.points.size() < 3) continue;

            float inv_extent = 1.0f / static_cast<float>(layer.extent);
            fan_triangulate(outer_ring.points, inv_extent,
                           batch.vertices, batch.indices);
            batch.layer_name = layer.name;
        }
    }

    return batch;
}

} // namespace render

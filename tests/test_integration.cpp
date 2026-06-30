// tests/test_integration.cpp — End-to-end integration tests
#include "style_engine.h"
#include "mvt_parser.h"
#include "render_data.h"
#include "osm_loader.h"
#include "building_data.h"

#include <gtest/gtest.h>
#include <fstream>
#include <vector>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

using namespace style;
using namespace mvt;
using namespace render;
using namespace osm;
using namespace bldg;

// ─── 1. End-to-End: MVT Parse → Render ──────────────────────────────────

TEST(Integration, MVTParseAndRender) {
    // Parse MVT file
    std::ifstream file("../data/test_roads.mvt", std::ios::binary | std::ios::ate);
    if (!file) {
        GTEST_SKIP() << "test_roads.mvt not found";
        return;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(static_cast<size_t>(size));
    file.read(buffer.data(), size);

    google::protobuf::io::ArrayInputStream array_input(buffer.data(),
                                                        static_cast<int>(size));
    google::protobuf::io::CodedInputStream coded_input(&array_input);
    Tile tile = parse_tile(&coded_input);

    // Extract lines
    auto line_batch = extract_lines(tile, "streets");
    // Verify we got some geometry
    if (!line_batch.vertices.empty()) {
        EXPECT_GT(line_batch.indices.size(), 0u);
        // Verify clip space range (allow small tolerance for features near tile edges)
        for (const auto& v : line_batch.vertices) {
            EXPECT_GE(v.x, -1.1f);
            EXPECT_LE(v.x, 1.1f);
            EXPECT_GE(v.y, -1.1f);
            EXPECT_LE(v.y, 1.1f);
        }
    }
}

TEST(Integration, MVTParseAndRenderPolygons) {
    std::ifstream file("../data/test_polygons.mvt", std::ios::binary | std::ios::ate);
    if (!file) {
        GTEST_SKIP() << "test_polygons.mvt not found";
        return;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(static_cast<size_t>(size));
    file.read(buffer.data(), size);

    google::protobuf::io::ArrayInputStream array_input(buffer.data(),
                                                        static_cast<int>(size));
    google::protobuf::io::CodedInputStream coded_input(&array_input);
    Tile tile = parse_tile(&coded_input);

    auto poly_batch = extract_polygons(tile, "");
    if (!poly_batch.vertices.empty()) {
        EXPECT_GT(poly_batch.indices.size(), 0u);
        for (const auto& v : poly_batch.vertices) {
            EXPECT_GE(v.x, -1.0f);
            EXPECT_LE(v.x, 1.0f);
        }
    }
}

// ─── 2. End-to-End: OSM Load → Extrude ──────────────────────────────────

TEST(Integration, OSMLoadAndExtrude) {
    if (!std::ifstream("../data/osm_data.json").good()) {
        GTEST_SKIP() << "osm_data.json not found";
        return;
    }
    auto data = load_osm_json("../data/osm_data.json");
    EXPECT_GT(data.buildings.size(), 0u);

    auto batch = extract_buildings(data.buildings);
    EXPECT_GT(batch.vertices.size(), 0u);
    EXPECT_GT(batch.indices.size(), 0u);

    // Verify vertex count is even (2 per footprint point)
    EXPECT_EQ(batch.vertices.size() % 2, 0u);
}

TEST(Integration, OSMDataBounds) {
    if (!std::ifstream("../data/osm_data.json").good()) {
        GTEST_SKIP() << "osm_data.json not found";
        return;
    }
    auto data = load_osm_json("../data/osm_data.json");

    // Compute bounds
    float min_x = 1e9f, min_y = 1e9f, max_x = -1e9f, max_y = -1e9f;
    auto update_bounds = [&](float x, float y) {
        if (x < min_x) min_x = x;
        if (y < min_y) min_y = y;
        if (x > max_x) max_x = x;
        if (y > max_y) max_y = y;
    };

    for (const auto& b : data.buildings) {
        for (const auto& p : b.footprint) {
            update_bounds(p.x, p.y);
        }
    }

    // Delhi data should be in Mercator space (0..65536)
    EXPECT_GT(min_x, 0.0f);
    EXPECT_GT(min_y, 0.0f);
    EXPECT_LT(max_x, 65536.0f);
    EXPECT_LT(max_y, 65536.0f);
}

// ─── 3. End-to-End: Style Matching ──────────────────────────────────────

TEST(Integration, StyleMatchAllLayers) {
    StyleEngine engine;
    ASSERT_TRUE(engine.loadFromJson("../data/style.json"));

    // Match all layer types
    auto bg = engine.matchRule("background", "background");
    auto tri = engine.matchRule("triangle", "fill");
    auto bldg = engine.matchRule("buildings", "fill-extrusion");
    auto roads = engine.matchRule("roads", "line");
    auto water = engine.matchRule("water", "fill");
    auto parks = engine.matchRule("parks", "fill");
    auto landuse = engine.matchRule("landuse", "fill");

    // All should return valid rules (not crash)
    (void)bg; (void)tri; (void)bldg; (void)roads;
    (void)water; (void)parks; (void)landuse;
}

// ─── 4. End-to-End: Full Pipeline ───────────────────────────────────────

TEST(Integration, FullPipeline) {
    // 1. Load style
    StyleEngine engine;
    ASSERT_TRUE(engine.loadFromJson("../data/style.json"));

    // 2. Load OSM data
    if (!std::ifstream("../data/osm_data.json").good()) {
        GTEST_SKIP() << "osm_data.json not found";
        return;
    }
    auto data = load_osm_json("../data/osm_data.json");

    // 3. Extract buildings
    auto bldg_batch = extract_buildings(data.buildings);
    EXPECT_GT(bldg_batch.indices.size(), 0u);

    // 4. Extract 2D fills
    auto park_rule = engine.matchRule("parks", "fill");
    auto water_rule = engine.matchRule("water", "fill");
    auto landuse_rule = engine.matchRule("landuse", "fill");

    auto fills = extract_fills_2d(
        data.parks, data.water_polygons, data.landuse,
        glm::vec3(park_rule.fill_color[0], park_rule.fill_color[1], park_rule.fill_color[2]),
        glm::vec3(water_rule.fill_color[0], water_rule.fill_color[1], water_rule.fill_color[2]),
        glm::vec3(landuse_rule.fill_color[0], landuse_rule.fill_color[1], landuse_rule.fill_color[2])
    );
    EXPECT_GE(fills.size(), 0u); // May be empty if no parks/water/landuse

    // 5. Compute zoom level
    float min_x = 1e9f, min_y = 1e9f, max_x = -1e9f, max_y = -1e9f;
    auto update_bounds = [&](float x, float y) {
        if (x < min_x) min_x = x;
        if (y < min_y) min_y = y;
        if (x > max_x) max_x = x;
        if (y > max_y) max_y = y;
    };

    for (const auto& b : data.buildings) {
        for (const auto& p : b.footprint) {
            update_bounds(p.x, p.y);
        }
    }

    float range_x = (max_x - min_x) * 0.5f;
    float range_y = (max_y - min_y) * 0.5f;
    float max_range = std::max(range_x, range_y);
    float zoom_level = 2.0f * 1.5f / max_range;

    // Verify zoom is reasonable
    EXPECT_GT(zoom_level, 0.0f);
    EXPECT_LT(zoom_level, 1.0f);
}

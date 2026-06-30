// tests/test_osm.cpp — Unit tests for OSM loader + Mercator projection
#include "osm_loader.h"

#include <gtest/gtest.h>
#include <cmath>

using namespace osm;

// ─── 1. Mercator Projection ─────────────────────────────────────────────

TEST(Mercator, LonToXCenter) {
    EXPECT_NEAR(lon_to_x(0.0, 65536.0), 32768.0f, 1.0f);
}

TEST(Mercator, LonToXLeft) {
    EXPECT_NEAR(lon_to_x(-180.0, 65536.0), 0.0f, 1.0f);
}

TEST(Mercator, LonToXRight) {
    EXPECT_NEAR(lon_to_x(180.0, 65536.0), 65536.0f, 1.0f);
}

TEST(Mercator, LatToYNorthPole) {
    float y = lat_to_y(85.0, 65536.0);
    EXPECT_LT(y, 5000.0f); // Should be near 0
}

TEST(Mercator, LatToYSouthPole) {
    float y = lat_to_y(-85.0, 65536.0);
    EXPECT_GT(y, 60000.0f); // Should be near world_height
}

TEST(Mercator, LatToYEquator) {
    float y = lat_to_y(0.0, 65536.0);
    EXPECT_NEAR(y, 32768.0f, 100.0f); // Approximately center
}

TEST(Mercator, LonLatToMercatorDelhi) {
    auto p = lonlat_to_mercator(77.2, 28.6, 65536.0, 65536.0);
    // Delhi should be in the upper-right quadrant
    EXPECT_GT(p.x, 30000.0f);
    EXPECT_GT(p.y, 20000.0f);
    EXPECT_LT(p.x, 50000.0f);
    EXPECT_LT(p.y, 40000.0f);
}

TEST(Mercator, Roundtrip) {
    // Verify lonlat → mercator → lonlat preserves values
    double lon_in = 77.2, lat_in = 28.6;
    double w = 65536.0;
    float mx = lon_to_x(lon_in, w);
    // Inverse: lon = mx/w * 360 - 180
    double lon_out = static_cast<double>(mx) / w * 360.0 - 180.0;
    EXPECT_NEAR(lon_out, lon_in, 0.01);
}

// ─── 2. JSON Loading ────────────────────────────────────────────────────

TEST(OSMLoader, LoadNonexistentFile) {
    auto data = load_osm_json("nonexistent.json");
    EXPECT_TRUE(data.buildings.empty());
    EXPECT_TRUE(data.roads.empty());
}

TEST(OSMLoader, LoadValidData) {
    if (!std::ifstream("data/osm_data.json").good()) {
        GTEST_SKIP() << "osm_data.json not found";
    }
    auto data = load_osm_json("data/osm_data.json");
    EXPECT_GT(data.buildings.size(), 0u);
}

TEST(OSMLoader, LoadMinimalJson) {
    const char* path = "/tmp/test_minimal_osm.json";
    {
        std::ofstream f(path);
        f << "{\"buildings\": [{\"id\": 1, \"polygon\": [[77.0, 28.0], [77.1, 28.0], [77.0, 28.1]]}]}";
    }
    auto data = load_osm_json(path);
    EXPECT_EQ(data.buildings.size(), 1u);
    EXPECT_EQ(data.roads.size(), 0u);
    std::remove(path);
}

TEST(OSMLoader, ParsePolygonNestedFormat) {
    // Test via the loader - create a minimal JSON and verify it loads correctly
    const char* path = "/tmp/test_polygon_format.json";
    {
        std::ofstream f(path);
        f << "{\"buildings\": [{\"id\": 1, \"polygon\": [[77.0, 28.0], [77.1, 28.0], [77.0, 28.1]]}]}";
    }
    auto data = load_osm_json(path);
    ASSERT_FALSE(data.buildings.empty());
    EXPECT_EQ(data.buildings[0].footprint.size(), 3u);
    std::remove(path);
}

TEST(OSMLoader, DefaultBuildingType) {
    const char* path = "/tmp/test_default_type.json";
    {
        std::ofstream f(path);
        f << "{\"buildings\": [{\"id\": 1, \"polygon\": [[77.0, 28.0], [77.1, 28.0], [77.0, 28.1]]}]}";
    }
    auto data = load_osm_json(path);
    if (!data.buildings.empty()) {
        EXPECT_EQ(data.buildings[0].type, "yes");
    }
    std::remove(path);
}

TEST(OSMLoader, DefaultRoadType) {
    const char* path = "/tmp/test_default_road.json";
    {
        std::ofstream f(path);
        f << "{\"roads\": [{\"id\": 1, \"line\": [[77.0, 28.0], [77.1, 28.0]]}]}";
    }
    auto data = load_osm_json(path);
    if (!data.roads.empty()) {
        EXPECT_EQ(data.roads[0].type, "residential");
    }
    std::remove(path);
}

TEST(OSMLoader, ParseBuildingFields) {
    const char* path = "/tmp/test_building_fields.json";
    {
        std::ofstream f(path);
        f << "{\"buildings\": [{\"id\": 42, \"name\": \"Test Bldg\", \"type\": \"commercial\", "
             "\"height\": 15.5, \"color\": [0.5, 0.6, 0.7], "
             "\"polygon\": [[77.0, 28.0], [77.1, 28.0], [77.0, 28.1]]}]}";
    }
    auto data = load_osm_json(path);
    ASSERT_FALSE(data.buildings.empty());
    EXPECT_EQ(data.buildings[0].id, 42);
    EXPECT_EQ(data.buildings[0].name, "Test Bldg");
    EXPECT_EQ(data.buildings[0].type, "commercial");
    EXPECT_NEAR(data.buildings[0].height, 15.5f, 0.1f);
    EXPECT_NEAR(data.buildings[0].color[0], 0.5f, 0.01f);
    EXPECT_NEAR(data.buildings[0].color[1], 0.6f, 0.01f);
    EXPECT_NEAR(data.buildings[0].color[2], 0.7f, 0.01f);
    std::remove(path);
}

TEST(OSMLoader, ParseRoadFields) {
    const char* path = "/tmp/test_road_fields.json";
    {
        std::ofstream f(path);
        f << "{\"roads\": [{\"id\": 1, \"name\": \"Main St\", \"type\": \"primary\", "
             "\"width\": 3.0, \"color\": [0.8, 0.7, 0.3], "
             "\"line\": [[77.0, 28.0], [77.1, 28.0], [77.2, 28.1]]}]}";
    }
    auto data = load_osm_json(path);
    ASSERT_FALSE(data.roads.empty());
    EXPECT_EQ(data.roads[0].id, 1);
    EXPECT_EQ(data.roads[0].name, "Main St");
    EXPECT_EQ(data.roads[0].type, "primary");
    EXPECT_NEAR(data.roads[0].width, 3.0f, 0.1f);
    EXPECT_EQ(data.roads[0].line.size(), 3u);
    std::remove(path);
}

TEST(OSMLoader, ParseParkFields) {
    const char* path = "/tmp/test_park.json";
    {
        std::ofstream f(path);
        f << "{\"parks\": [{\"id\": 5, \"name\": \"Central Park\", \"polygon\": "
             "[[77.0, 28.0], [77.1, 28.0], [77.05, 28.05]]}]}";
    }
    auto data = load_osm_json(path);
    ASSERT_FALSE(data.parks.empty());
    EXPECT_EQ(data.parks[0].id, 5);
    EXPECT_EQ(data.parks[0].name, "Central Park");
    EXPECT_EQ(data.parks[0].polygon.size(), 3u);
    std::remove(path);
}

TEST(OSMLoader, ParseWaterFields) {
    const char* path = "/tmp/test_water.json";
    {
        std::ofstream f(path);
        f << "{\"water_polygons\": [{\"id\": 1, \"name\": \"River\", "
             "\"polygon\": [[77.0, 28.0], [77.1, 28.0], [77.05, 28.05]]}]}";
    }
    auto data = load_osm_json(path);
    ASSERT_FALSE(data.water_polygons.empty());
    EXPECT_EQ(data.water_polygons[0].id, 1);
    EXPECT_EQ(data.water_polygons[0].name, "River");
    std::remove(path);
}

TEST(OSMLoader, ParseLanduseFields) {
    const char* path = "/tmp/test_landuse.json";
    {
        std::ofstream f(path);
        f << "{\"landuse\": [{\"id\": 3, \"name\": \"Forest\", \"polygon\": "
             "[[77.0, 28.0], [77.1, 28.0], [77.05, 28.05]]}]}";
    }
    auto data = load_osm_json(path);
    ASSERT_FALSE(data.landuse.empty());
    EXPECT_EQ(data.landuse[0].id, 3);
    EXPECT_EQ(data.landuse[0].name, "Forest");
    std::remove(path);
}

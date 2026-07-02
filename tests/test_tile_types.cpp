#include <gtest/gtest.h>
#include <unordered_map>
#include <map_renderer/tile_id.h>
#include <map_renderer/osm_types.h>

// ── TileId tests ──────────────────────────────────────────────────────

TEST(TileIdTest, Equality) {
    map_renderer::TileId a{8, 5, 3};
    map_renderer::TileId b{8, 5, 3};
    map_renderer::TileId c{8, 5, 4};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(TileIdTest, TriviallyCopyable) {
    EXPECT_TRUE(std::is_trivially_copyable_v<map_renderer::TileId>);
}

TEST(TileIdTest, HashWorksInUnorderedMap) {
    std::unordered_map<map_renderer::TileId, int, map_renderer::TileId::Hash> m;
    map_renderer::TileId t1{8, 5, 3};
    map_renderer::TileId t2{12, 10, 20};
    m[t1] = 42;
    m[t2] = 99;
    EXPECT_EQ(m[t1], 42);
    EXPECT_EQ(m[t2], 99);
}

TEST(TileIdTest, HashDifferentKeysDiffer) {
    map_renderer::TileId::Hash h;
    map_renderer::TileId t1{8, 5, 3};
    map_renderer::TileId t2{8, 5, 4};
    map_renderer::TileId t3{12, 0, 0};
    auto h1 = h(t1);
    auto h2 = h(t2);
    auto h3 = h(t3);
    EXPECT_NE(h1, h2);
    EXPECT_NE(h1, h3);
}

TEST(TileIdTest, HashLargeZoom) {
    map_renderer::TileId::Hash h;
    map_renderer::TileId t{20, 1000000, 1000000};
    auto val = h(t);
    (void)val;  // just verifying it compiles without crash
    SUCCEED();
}

// ── OSM types tests ───────────────────────────────────────────────────

TEST(OsmTypesTest, PointDefault) {
    map_renderer::Point p;
    EXPECT_FLOAT_EQ(p.x, 0.0f);
    EXPECT_FLOAT_EQ(p.z, 0.0f);
}

TEST(OsmTypesTest, BuildingDefault) {
    map_renderer::Building b;
    EXPECT_EQ(b.id, 0);
    EXPECT_TRUE(b.footprint.empty());
    EXPECT_FLOAT_EQ(b.height, 0.0f);
}

TEST(OsmTypesTest, RoadDefault) {
    map_renderer::Road r;
    EXPECT_EQ(r.id, 0);
    EXPECT_TRUE(r.line.empty());
    EXPECT_TRUE(r.type.empty());
    EXPECT_FLOAT_EQ(r.width, 6.0f);
}

TEST(OsmTypesTest, PolygonFeatureDefault) {
    map_renderer::PolygonFeature pf;
    EXPECT_TRUE(pf.polygon.empty());
    EXPECT_TRUE(pf.type.empty());
}

TEST(OsmTypesTest, TileDataDefault) {
    map_renderer::TileData td;
    EXPECT_FALSE(td.uploaded);
    EXPECT_EQ(td.vao, 0u);
    EXPECT_EQ(td.vbo, 0u);
    EXPECT_EQ(td.water_range.count, 0u);
    EXPECT_EQ(td.park_range.count, 0u);
    EXPECT_EQ(td.landuse_range.count, 0u);
    EXPECT_EQ(td.road_range.count, 0u);
    EXPECT_EQ(td.building_range.count, 0u);
}

TEST(OsmTypesTest, DrawRangeDefault) {
    map_renderer::TileData::DrawRange dr;
    EXPECT_EQ(dr.offset, 0u);
    EXPECT_EQ(dr.count, 0u);
}

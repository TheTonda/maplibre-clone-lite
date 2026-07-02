#include <gtest/gtest.h>
#include <map_renderer/osm_loader.h>
#include <map_renderer/tile_loader.h>
#include <map_renderer/tile_cache.h>
#include <osm_data.pb.h>
#include <zstd.h>

// ── Helpers ───────────────────────────────────────────────────────────

static std::vector<uint8_t> make_tile_proto(int buildings, int roads, int polygons) {
    map_renderer_pb::Tile tile;
    tile.set_zoom(12);
    tile.set_tile_x(100);
    tile.set_tile_y(200);
    tile.set_center_lat(28.6);
    tile.set_center_lon(77.2);
    tile.set_building_count(buildings);
    tile.set_road_count(roads);
    tile.set_polygon_count(polygons);

    for (int i = 0; i < buildings; ++i) {
        auto* b = tile.add_buildings();
        b->set_id(1000 + i);
        b->set_height_m(10.0f + i);
        auto* pt = b->add_footprint();
        pt->set_x(static_cast<float>(i) * 10.0f);
        pt->set_z(static_cast<float>(i) * 5.0f);
        pt = b->add_footprint();
        pt->set_x(static_cast<float>(i) * 10.0f + 1.0f);
        pt->set_z(static_cast<float>(i) * 5.0f + 1.0f);
        pt = b->add_footprint();
        pt->set_x(static_cast<float>(i) * 10.0f + 0.5f);
        pt->set_z(static_cast<float>(i) * 5.0f + 2.0f);
    }

    for (int i = 0; i < roads; ++i) {
        auto* r = tile.add_roads();
        r->set_id(2000 + i);
        r->set_type("residential");
        r->set_width_m(6.0f);
        auto* pt = r->add_line();
        pt->set_x(0.0f);
        pt->set_z(0.0f);
        pt = r->add_line();
        pt->set_x(100.0f + static_cast<float>(i));
        pt->set_z(50.0f + static_cast<float>(i));
    }

    for (int i = 0; i < polygons; ++i) {
        auto* p = tile.add_polygons();
        p->set_type("water");
        auto* pt = p->add_polygon();
        pt->set_x(0.0f);
        pt->set_z(0.0f);
        pt = p->add_polygon();
        pt->set_x(10.0f);
        pt->set_z(0.0f);
        pt = p->add_polygon();
        pt->set_x(10.0f);
        pt->set_z(10.0f);
        pt = p->add_polygon();
        pt->set_x(0.0f);
        pt->set_z(10.0f);
    }

    std::string raw;
    tile.SerializeToString(&raw);
    return std::vector<uint8_t>(raw.begin(), raw.end());
}

// ── OSMLoader tests ──────────────────────────────────────────────────

TEST(OSMLoaderTest, EmptyTile) {
    auto bytes = make_tile_proto(0, 0, 0);
    map_renderer::TileData td;
    bool ok = map_renderer::OSMLoader::deserialize(bytes, {12, 100, 200}, 28.5, 77.2, td);
    EXPECT_TRUE(ok);
    EXPECT_EQ(td.buildings.size(), 0u);
    EXPECT_EQ(td.roads.size(), 0u);
    EXPECT_EQ(td.polygons.size(), 0u);
}

TEST(OSMLoaderTest, BuildingTile) {
    auto bytes = make_tile_proto(3, 0, 0);
    map_renderer::TileData td;
    bool ok = map_renderer::OSMLoader::deserialize(bytes, {12, 100, 200}, 28.5, 77.2, td);
    EXPECT_TRUE(ok);
    ASSERT_EQ(td.buildings.size(), 3u);
    EXPECT_EQ(td.buildings[0].id, 1000);
    EXPECT_FLOAT_EQ(td.buildings[0].height, 10.0f);
    EXPECT_EQ(td.buildings[0].footprint.size(), 3u);
}

TEST(OSMLoaderTest, RoadTile) {
    auto bytes = make_tile_proto(0, 2, 0);
    map_renderer::TileData td;
    bool ok = map_renderer::OSMLoader::deserialize(bytes, {12, 100, 200}, 28.5, 77.2, td);
    EXPECT_TRUE(ok);
    ASSERT_EQ(td.roads.size(), 2u);
    EXPECT_EQ(td.roads[0].id, 2000);
    EXPECT_EQ(td.roads[0].type, "residential");
    EXPECT_FLOAT_EQ(td.roads[0].width, 6.0f);
    EXPECT_EQ(td.roads[0].line.size(), 2u);
}

TEST(OSMLoaderTest, PolygonTile) {
    auto bytes = make_tile_proto(0, 0, 1);
    map_renderer::TileData td;
    bool ok = map_renderer::OSMLoader::deserialize(bytes, {12, 100, 200}, 28.5, 77.2, td);
    EXPECT_TRUE(ok);
    ASSERT_EQ(td.polygons.size(), 1u);
    EXPECT_EQ(td.polygons[0].type, "water");
    EXPECT_EQ(td.polygons[0].polygon.size(), 4u);
}

TEST(OSMLoaderTest, WorldOffset) {
    auto bytes = make_tile_proto(0, 0, 0);
    map_renderer::TileData td;
    bool ok = map_renderer::OSMLoader::deserialize(bytes, {12, 100, 200}, 28.5, 77.2, td);
    EXPECT_TRUE(ok);
    // Tile center at (28.6, 77.2), ref at (28.5, 77.2)
    // dz = R * (28.6 - 28.5) * pi/180 ≈ 6371000 * 0.1 * 0.0174533 ≈ 11119
    EXPECT_NEAR(td.world_offset_z, 11119.0f, 1.0f);
    // dx ≈ 0 (same longitude)
    EXPECT_NEAR(td.world_offset_x, 0.0f, 10.0f);
}

TEST(OSMLoaderTest, CorruptData) {
    std::vector<uint8_t> corrupt = {0xFF, 0xFF, 0xFF, 0xFF};
    map_renderer::TileData td;
    bool ok = map_renderer::OSMLoader::deserialize(corrupt, {12, 0, 0}, 0.0, 0.0, td);
    EXPECT_FALSE(ok);
}

// ── TileLoader tests ─────────────────────────────────────────────────

TEST(TileLoaderTest, FilePath) {
    // Test path computation
    std::string path = map_renderer::TileLoader::tile_path("/tiles", {8, 5, 3});
    EXPECT_EQ(path, "/tiles/8/5/3.bin");
}

TEST(TileLoaderTest, StartStop) {
    map_renderer::TileCache cache(64);
    map_renderer::TileLoader loader("data/tiles/new_delhi", cache, 28.589, 77.2375);
    loader.start();
    loader.stop();
    // Should not crash
    SUCCEED();
}

TEST(TileLoaderTest, MissingFile) {
    map_renderer::TileCache cache(64);
    map_renderer::TileLoader loader("data/tiles/new_delhi", cache, 28.589, 77.2375);
    // Request a tile that doesn't exist
    loader.request_tiles({{{8, 9999, 9999}}});
    loader.start();
    // Give it time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    loader.stop();
    // Should not crash, tile just won't be in cache
    EXPECT_EQ(cache.size(), 0u);
}

#include <gtest/gtest.h>
#include "data/osm_loader.h"
#include "data/osm_types.h"

#include "osm_data.pb.h"

// -----------------------------------------------------------------------
// Helper: build a minimal valid protobuf in memory
// -----------------------------------------------------------------------

static std::string make_minimal_proto() {
    osm_proto::OSMDataProto proto;
    proto.set_schema_version(2);
    proto.set_center_x(0.0);
    proto.set_center_z(0.0);
    proto.set_min_x(-100.0);
    proto.set_min_z(-100.0);
    proto.set_max_x(100.0);
    proto.set_max_z(100.0);

    // One building
    auto* b = proto.add_buildings();
    b->set_id(1);
    b->set_height_m(12.0);
    b->set_height_source("tag");
    b->set_type("house");
    {
        auto* p1 = b->add_footprint(); p1->set_x(0); p1->set_z(0);
        auto* p2 = b->add_footprint(); p2->set_x(10); p2->set_z(0);
        auto* p3 = b->add_footprint(); p3->set_x(10); p3->set_z(10);
        auto* p4 = b->add_footprint(); p4->set_x(0); p4->set_z(10);
    }

    // One road
    auto* r = proto.add_roads();
    r->set_id(42);
    r->set_type("primary");
    r->set_width_m(8.0);
    {
        auto* p1 = r->add_line(); p1->set_x(0); p1->set_z(0);
        auto* p2 = r->add_line(); p2->set_x(100); p2->set_z(0);
    }

    // One park
    auto* p = proto.add_parks();
    p->set_type("park");
    {
        auto* pt1 = p->add_polygon(); pt1->set_x(50); pt1->set_z(50);
        auto* pt2 = p->add_polygon(); pt2->set_x(60); pt2->set_z(50);
        auto* pt3 = p->add_polygon(); pt3->set_x(60); pt3->set_z(60);
    }

    return proto.SerializeAsString();
}

// -----------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------

TEST(OSMLoaderTest, LoadMinimalProto) {
    auto serialised = make_minimal_proto();
    auto data = OSMLoader::load_from_string(serialised);

    EXPECT_EQ(data.buildings.size(), 1u);
    EXPECT_EQ(data.roads.size(), 1u);
    EXPECT_EQ(data.parks.size(), 1u);
    EXPECT_TRUE(data.has_data());
}

TEST(OSMLoaderTest, BuildingHeight) {
    auto serialised = make_minimal_proto();
    auto data = OSMLoader::load_from_string(serialised);

    ASSERT_EQ(data.buildings.size(), 1u);
    EXPECT_FLOAT_EQ(data.buildings[0].height_m, 12.0f);
    EXPECT_EQ(data.buildings[0].height_source, osm::HeightSource::Tag);
    EXPECT_EQ(data.buildings[0].type, "house");
}

TEST(OSMLoaderTest, RoadDetails) {
    auto serialised = make_minimal_proto();
    auto data = OSMLoader::load_from_string(serialised);

    ASSERT_EQ(data.roads.size(), 1u);
    EXPECT_EQ(data.roads[0].id, 42);
    EXPECT_EQ(data.roads[0].type, "primary");
    EXPECT_FLOAT_EQ(data.roads[0].width_m, 8.0f);
    EXPECT_EQ(data.roads[0].line.size(), 2u);
}

TEST(OSMLoaderTest, Bounds) {
    auto serialised = make_minimal_proto();
    auto data = OSMLoader::load_from_string(serialised);

    EXPECT_DOUBLE_EQ(data.min_x, -100.0);
    EXPECT_DOUBLE_EQ(data.max_x, 100.0);
}

TEST(OSMLoaderTest, EmptyFile) {
    auto data = OSMLoader::load_from_string("");
    EXPECT_FALSE(data.has_data());
    EXPECT_EQ(data.buildings.size(), 0u);
}

TEST(OSMLoaderTest, SchemaVersionMismatch) {
    osm_proto::OSMDataProto proto;
    proto.set_schema_version(999);  // wrong version
    proto.set_center_x(0);
    proto.set_center_z(0);
    auto serialised = proto.SerializeAsString();
    // Should not crash; returns empty
    auto data = OSMLoader::load_from_string(serialised);
    EXPECT_FALSE(data.has_data())
        << "Should handle version mismatch gracefully";
}

TEST(OSMLoaderTest, PolygonFeatures) {
    auto serialised = make_minimal_proto();
    auto data = OSMLoader::load_from_string(serialised);

    ASSERT_EQ(data.water.size(), 0u);
    ASSERT_EQ(data.landuse.size(), 0u);
    ASSERT_EQ(data.parks.size(), 1u);
    EXPECT_EQ(data.parks[0].type, "park");
    EXPECT_EQ(data.parks[0].polygon.size(), 3u);
}

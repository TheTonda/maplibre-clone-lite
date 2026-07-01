#include <gtest/gtest.h>
#include "data/geometry_builder.h"

// ======================================================================
// Helper: make a square building
// ======================================================================

static osm::Building make_square_building(float height = 10.0f) {
    osm::Building b;
    b.id = 1;
    b.height_m = height;
    b.type = "house";
    b.footprint = {
        {0, 0}, {10, 0}, {10, 10}, {0, 10}
    };
    return b;
}

static osm::Building make_triangle_building() {
    osm::Building b;
    b.id = 2;
    b.height_m = 5.0f;
    b.type = "shed";
    b.footprint = {
        {0, 0}, {10, 0}, {5, 10}
    };
    return b;
}

// ======================================================================
// Tests – Ground
// ======================================================================

TEST(GeometryBuilderTest, Ground) {
    auto g = GeometryBuilder::build_ground(100.0f);
    EXPECT_EQ(g.vertices.size(), 4u);
    EXPECT_EQ(g.indices.size(), 6u);
    EXPECT_FLOAT_EQ(g.vertices[0].pos.x, -100.0f);
    EXPECT_FLOAT_EQ(g.vertices[2].pos.y,  100.0f);
}

// ======================================================================
// Tests – Triangulation
// ======================================================================

TEST(GeometryBuilderTest, TriangulateTriangle) {
    std::vector<osm::Point> tri = {{0, 0}, {10, 0}, {5, 10}};
    std::vector<glm::vec2> verts;
    auto idxs = GeometryBuilder::triangulate_polygon(tri, verts);

    EXPECT_EQ(verts.size(), 3u);
    EXPECT_EQ(idxs.size(), 3u);  // one triangle
}

TEST(GeometryBuilderTest, TriangulateSquare) {
    std::vector<osm::Point> sq = {{0, 0}, {10, 0}, {10, 10}, {0, 10}};
    std::vector<glm::vec2> verts;
    auto idxs = GeometryBuilder::triangulate_polygon(sq, verts);

    EXPECT_EQ(verts.size(), 4u);
    EXPECT_EQ(idxs.size(), 6u);  // two triangles
}

// ======================================================================
// Tests – Buildings
// ======================================================================

TEST(GeometryBuilderTest, SquareBuilding) {
    auto b = make_square_building(10.0f);
    auto mesh = GeometryBuilder::build_building(b);

    // 4 walls × 4 verts each = 16 wall verts
    // 2 caps × 4 verts each = 8 cap verts
    // Total = 24
    EXPECT_EQ(mesh.vertices.size(), 24u);

    // Indices: 2 caps × 6 = 12, 4 walls × 6 = 24 → 36
    EXPECT_EQ(mesh.indices.size(), 36u);

    // Check normals: top cap vertices have +Y
    for (size_t i = 4; i < 8; ++i) {  // top cap is verts 4-7
        EXPECT_FLOAT_EQ(mesh.vertices[i].normal.y, 1.0f);
    }
}

TEST(GeometryBuilderTest, TriangleBuilding) {
    auto b = make_triangle_building();
    auto mesh = GeometryBuilder::build_building(b);

    // 3 walls × 4 verts = 12, 2 caps × 3 = 6 → 18
    EXPECT_EQ(mesh.vertices.size(), 18u);

    // 3 walls × 6 = 18, 2 caps × 3 = 6 → 24
    EXPECT_EQ(mesh.indices.size(), 24u);
}

// ======================================================================
// Tests – Roads
// ======================================================================

TEST(GeometryBuilderTest, StraightRoad) {
    osm::Road r;
    r.id = 1;
    r.type = "primary";
    r.width_m = 8.0f;
    r.line = {{0, 0}, {100, 0}};

    auto mesh = GeometryBuilder::build_road_quad(r);
    // 2 segments × 4 verts = 8... wait, actually it's 1 segment
    // 1 segment = 4 verts
    EXPECT_EQ(mesh.vertices.size(), 4u);
    EXPECT_EQ(mesh.indices.size(), 6u);  // 2 triangles

    // Y coordinates should all be 0
    for (const auto& v : mesh.vertices)
        EXPECT_FLOAT_EQ(v.pos.y, 0.0f);
}

TEST(GeometryBuilderTest, RoadWidth) {
    osm::Road r;
    r.id = 2;
    r.type = "service";
    r.width_m = 3.0f;
    r.line = {{0, 0}, {10, 0}};

    auto mesh = GeometryBuilder::build_road_quad(r);
    // Vertices are at ±half_width from centre
    // Road runs along X axis, so width extends in Z
    EXPECT_NEAR(mesh.vertices[0].pos.z, -1.5f, 0.001f);
    EXPECT_NEAR(mesh.vertices[1].pos.z,  1.5f, 0.001f);
}

// ======================================================================
// Tests – Bulk build
// ======================================================================

TEST(GeometryBuilderTest, BuildAll) {
    osm::OSMData data;
    data.buildings.push_back(make_square_building());
    data.buildings.push_back(make_triangle_building());

    auto g = GeometryBuilder::build_all(data, 100.0f);

    EXPECT_EQ(g.buildings.vertices.size(), 24u + 18u);  // both buildings
    EXPECT_EQ(g.ground.vertices.size(), 4u);
    EXPECT_TRUE(g.roads_primary.vertices.empty());  // no roads in data
}

#include <gtest/gtest.h>

#include <cmath>
#include <map_renderer/geometry_builder.h>
#include <map_renderer/osm_types.h>

using map_renderer::BuiltGeometry;
using map_renderer::Building;
using map_renderer::GeometryBuilder;
using map_renderer::Point;
using map_renderer::PolygonFeature;
using map_renderer::Road;
using map_renderer::TileData;

namespace {

Point P(float x, float z) { return Point{x, z}; }

PolygonFeature make_poly(const std::string& type,
                         std::initializer_list<Point> pts) {
    PolygonFeature pf;
    pf.type = type;
    pf.polygon = std::vector<Point>(pts);
    return pf;
}

} // namespace

// ── Ear-clipping triangulation ────────────────────────────────────────

TEST(GeometryBuilderTest, ConvexSquareTriangulates) {
    // CCW unit square -> 2 triangles -> 6 vertices.
    TileData tile;
    tile.polygons.push_back(make_poly("water", {
        P(0.0f, 0.0f), P(1.0f, 0.0f), P(1.0f, 1.0f), P(0.0f, 1.0f)
    }));

    BuiltGeometry geom = GeometryBuilder::build_tile(tile);

    // (n-2)*3 = 2*3 = 6 vertices for a 4-gon.
    EXPECT_EQ(geom.water.count, 6u);
    EXPECT_EQ(geom.water.offset, 0u);
    // 6 vertices * 2 floats = 12 floats.
    EXPECT_EQ(geom.vertices.size(), 12u);
    // All other ranges empty.
    EXPECT_EQ(geom.park.count, 0u);
    EXPECT_EQ(geom.landuse.count, 0u);
    EXPECT_EQ(geom.road.count, 0u);
    EXPECT_EQ(geom.building.count, 0u);
}

TEST(GeometryBuilderTest, ConvexPentagonVertexCount) {
    // Regular-ish CCW pentagon -> 3 triangles -> 9 vertices.
    TileData tile;
    tile.polygons.push_back(make_poly("park", {
        P(0.0f, 0.0f), P(2.0f, 0.0f), P(3.0f, 2.0f),
        P(1.0f, 4.0f), P(-1.0f, 2.0f)
    }));

    BuiltGeometry geom = GeometryBuilder::build_tile(tile);

    EXPECT_EQ(geom.park.count, 9u);  // (5-2)*3
    EXPECT_EQ(geom.park.offset, 0u);
    EXPECT_EQ(geom.vertices.size(), 18u);
}

TEST(GeometryBuilderTest, ConcaveLShapeTriangulates) {
    // Concave "L" (CCW). 6 vertices -> 4 triangles -> 12 vertices.
    //   (0,0)-(2,0)-(2,2)-(1,2)-(1,1)-(0,1)
    TileData tile;
    tile.polygons.push_back(make_poly("landuse", {
        P(0.0f, 0.0f), P(2.0f, 0.0f), P(2.0f, 2.0f),
        P(1.0f, 2.0f), P(1.0f, 1.0f), P(0.0f, 1.0f)
    }));

    BuiltGeometry geom = GeometryBuilder::build_tile(tile);

    EXPECT_EQ(geom.landuse.count, 12u);  // (6-2)*3
    EXPECT_EQ(geom.landuse.offset, 0u);
    EXPECT_EQ(geom.vertices.size(), 24u);
}

TEST(GeometryBuilderTest, WindingClockwiseIsCorrected) {
    // Same square as ConvexSquareTriangulates but wound clockwise.
    // The builder must reverse it to CCW and still produce 6 vertices.
    TileData tile;
    tile.polygons.push_back(make_poly("water", {
        P(0.0f, 0.0f), P(0.0f, 1.0f), P(1.0f, 1.0f), P(1.0f, 0.0f)
    }));

    BuiltGeometry geom = GeometryBuilder::build_tile(tile);

    EXPECT_EQ(geom.water.count, 6u);
    EXPECT_EQ(geom.vertices.size(), 12u);
}

// ── Degenerate polygons ───────────────────────────────────────────────

TEST(GeometryBuilderTest, DegenerateFewerThanThreePoints) {
    TileData tile;
    tile.polygons.push_back(make_poly("water", {P(0.0f, 0.0f), P(1.0f, 0.0f)}));

    BuiltGeometry geom = GeometryBuilder::build_tile(tile);

    EXPECT_EQ(geom.water.count, 0u);
    EXPECT_TRUE(geom.vertices.empty());
}

TEST(GeometryBuilderTest, DegenerateEmptyPolygon) {
    TileData tile;
    tile.polygons.push_back(make_poly("water", {}));

    BuiltGeometry geom = GeometryBuilder::build_tile(tile);

    EXPECT_EQ(geom.water.count, 0u);
    EXPECT_TRUE(geom.vertices.empty());
}

TEST(GeometryBuilderTest, DegenerateCollinearNoCrash) {
    // Four collinear points: no ear is convex, fan fallback is used.
    TileData tile;
    tile.polygons.push_back(make_poly("landuse", {
        P(0.0f, 0.0f), P(1.0f, 0.0f), P(2.0f, 0.0f), P(3.0f, 0.0f)
    }));

    BuiltGeometry geom = GeometryBuilder::build_tile(tile);

    // Fan produces (n-2)*3 = 6 degenerate (zero-area) triangles -> "minimal".
    EXPECT_EQ(geom.landuse.count, 6u);
    EXPECT_EQ(geom.vertices.size(), 12u);
}

// ── Road quad generation ──────────────────────────────────────────────

TEST(GeometryBuilderTest, RoadQuadSingleSegment) {
    // Segment (0,0)->(10,0), width 2.
    // dir=(1,0), perp=(0,1), half_w=1.
    // v0=(0,1)  v1=(0,-1)  v2=(10,1)  v3=(10,-1)
    TileData tile;
    Road r;
    r.width = 2.0f;
    r.line = {P(0.0f, 0.0f), P(10.0f, 0.0f)};
    tile.roads.push_back(r);

    BuiltGeometry geom = GeometryBuilder::build_tile(tile);

    // 1 segment -> 2 triangles -> 6 vertices.
    EXPECT_EQ(geom.road.count, 6u);
    EXPECT_EQ(geom.road.offset, 0u);
    EXPECT_EQ(geom.vertices.size(), 12u);

    // v0 (left start) = (0, 1), v1 (right start) = (0, -1).
    ASSERT_GE(geom.vertices.size(), 4u);
    EXPECT_FLOAT_EQ(geom.vertices[0], 0.0f);   // v0.x
    EXPECT_FLOAT_EQ(geom.vertices[1], 1.0f);   // v0.z
    EXPECT_FLOAT_EQ(geom.vertices[2], 0.0f);   // v1.x
    EXPECT_FLOAT_EQ(geom.vertices[3], -1.0f);  // v1.z

    // Width = distance between v0 and v1 along perp = 2.
    const float width = std::fabs(geom.vertices[1] - geom.vertices[3]);
    EXPECT_FLOAT_EQ(width, 2.0f);
}

TEST(GeometryBuilderTest, RoadQuadMultiSegment) {
    // Two segments -> 2 quads -> 4 triangles -> 12 vertices.
    TileData tile;
    Road r;
    r.width = 4.0f;
    r.line = {P(0.0f, 0.0f), P(10.0f, 0.0f), P(10.0f, 10.0f)};
    tile.roads.push_back(r);

    BuiltGeometry geom = GeometryBuilder::build_tile(tile);

    EXPECT_EQ(geom.road.count, 12u);
    EXPECT_EQ(geom.vertices.size(), 24u);
}

TEST(GeometryBuilderTest, RoadQuadDegenerateSegmentSkipped) {
    // Two points coincident -> degenerate segment skipped, no vertices.
    TileData tile;
    Road r;
    r.width = 2.0f;
    r.line = {P(5.0f, 5.0f), P(5.0f, 5.0f)};
    tile.roads.push_back(r);

    BuiltGeometry geom = GeometryBuilder::build_tile(tile);

    EXPECT_EQ(geom.road.count, 0u);
    EXPECT_TRUE(geom.vertices.empty());
}

// ── Draw ranges & ordering ────────────────────────────────────────────

TEST(GeometryBuilderTest, DrawRangesCorrectOrder) {
    // One of each feature: water sq, park sq, landuse sq, road (1 seg),
    // building sq. Each square = 6 vertices, road = 6 vertices.
    TileData tile;
    tile.polygons.push_back(make_poly("water", {
        P(0.0f, 0.0f), P(1.0f, 0.0f), P(1.0f, 1.0f), P(0.0f, 1.0f)}));
    tile.polygons.push_back(make_poly("park", {
        P(0.0f, 0.0f), P(1.0f, 0.0f), P(1.0f, 1.0f), P(0.0f, 1.0f)}));
    tile.polygons.push_back(make_poly("landuse", {
        P(0.0f, 0.0f), P(1.0f, 0.0f), P(1.0f, 1.0f), P(0.0f, 1.0f)}));

    Road r;
    r.width = 2.0f;
    r.line = {P(0.0f, 0.0f), P(10.0f, 0.0f)};
    tile.roads.push_back(r);

    Building b;
    b.footprint = {P(0.0f, 0.0f), P(1.0f, 0.0f), P(1.0f, 1.0f), P(0.0f, 1.0f)};
    tile.buildings.push_back(b);

    BuiltGeometry geom = GeometryBuilder::build_tile(tile);

    // Order: water, park, landuse, road, building.
    EXPECT_EQ(geom.water.offset, 0u);
    EXPECT_EQ(geom.water.count, 6u);

    EXPECT_EQ(geom.park.offset, 6u);
    EXPECT_EQ(geom.park.count, 6u);

    EXPECT_EQ(geom.landuse.offset, 12u);
    EXPECT_EQ(geom.landuse.count, 6u);

    EXPECT_EQ(geom.road.offset, 18u);
    EXPECT_EQ(geom.road.count, 6u);

    EXPECT_EQ(geom.building.offset, 24u);
    EXPECT_EQ(geom.building.count, 6u);

    // 30 vertices * 2 floats = 60 floats, contiguous.
    EXPECT_EQ(geom.vertices.size(), 60u);
}

TEST(GeometryBuilderTest, EmptyTileAllRangesZero) {
    TileData tile;
    BuiltGeometry geom = GeometryBuilder::build_tile(tile);

    EXPECT_EQ(geom.water.offset, 0u);
    EXPECT_EQ(geom.water.count, 0u);
    EXPECT_EQ(geom.park.offset, 0u);
    EXPECT_EQ(geom.park.count, 0u);
    EXPECT_EQ(geom.landuse.offset, 0u);
    EXPECT_EQ(geom.landuse.count, 0u);
    EXPECT_EQ(geom.road.offset, 0u);
    EXPECT_EQ(geom.road.count, 0u);
    EXPECT_EQ(geom.building.offset, 0u);
    EXPECT_EQ(geom.building.count, 0u);
    EXPECT_TRUE(geom.vertices.empty());
}

TEST(GeometryBuilderTest, UnknownPolygonTypeProducesNothing) {
    // Polygons whose type isn't water/park/landuse are dropped (no range).
    TileData tile;
    tile.polygons.push_back(make_poly("forest", {
        P(0.0f, 0.0f), P(1.0f, 0.0f), P(1.0f, 1.0f)}));

    BuiltGeometry geom = GeometryBuilder::build_tile(tile);

    EXPECT_EQ(geom.water.count, 0u);
    EXPECT_EQ(geom.park.count, 0u);
    EXPECT_EQ(geom.landuse.count, 0u);
    EXPECT_TRUE(geom.vertices.empty());
}

// tests/test_render.cpp — Unit tests for render_data (MVT → GPU conversion)
#include "render_data.h"

#include <gtest/gtest.h>
#include <cmath>

using namespace render;
using namespace mvt;

// ─── 1. Line String Decoding ────────────────────────────────────────────

TEST(DecodeLineString, SingleLine) {
    // Simple 2-point line: MoveTo(0,0), LineTo(10,10)
    // Commands: MoveTo with count=1 → cmd=9 (1<<3 | 1), then 2 coords
    // LineTo with count=1 → cmd=10 (1<<3 | 2), then 2 coords
    // Coordinates are zigzag-encoded: encode(v) = v << 1 for positive values
    std::vector<uint32_t> geometry = {
        9, 0, 0,      // MoveTo count=1: dx=zigzag(0)=0, dy=zigzag(0)=0 → (0,0)
        10, 20, 20,   // LineTo count=1: dx=zigzag(20)=10, dy=zigzag(20)=10 → (10,10)
    };
    auto segments = decode_linestring_geometry(geometry, 4096);
    EXPECT_EQ(segments.size(), 1u);
    EXPECT_EQ(segments[0].points.size(), 2u);
    EXPECT_EQ(segments[0].points[0], std::make_pair(0, 0));
    EXPECT_EQ(segments[0].points[1], std::make_pair(10, 10));
}

TEST(DecodeLineString, MultiSegment) {
    // Two separate line segments
    // Coordinates are zigzag-encoded
    std::vector<uint32_t> geometry = {
        9, 0, 0,      // MoveTo(0,0)
        10, 20, 0,    // LineTo(10,0) — dx=zigzag(20)=10, dy=zigzag(0)=0
        9, 40, 40,    // MoveTo(20,20) — dx=zigzag(40)=20, dy=zigzag(40)=20
        10, 10, 10,   // LineTo(25,25) — dx=zigzag(10)=5, dy=zigzag(10)=5
    };
    auto segments = decode_linestring_geometry(geometry, 4096);
    EXPECT_EQ(segments.size(), 2u);
    EXPECT_EQ(segments[0].points.size(), 2u);
    EXPECT_EQ(segments[1].points.size(), 2u);
}

TEST(DecodeLineString, ClosePath) {
    // Closed path: MoveTo(0,0), LineTo(10,0), ClosePath
    // Coordinates are zigzag-encoded
    std::vector<uint32_t> geometry = {
        9, 0, 0,      // MoveTo(0,0)
        10, 20, 0,    // LineTo(10,0) — dx=zigzag(20)=10, dy=zigzag(0)=0
        15,           // ClosePath (count=1, cmd_id=7)
    };
    auto segments = decode_linestring_geometry(geometry, 4096);
    EXPECT_EQ(segments.size(), 1u);
    EXPECT_EQ(segments[0].points.size(), 3u); // 2 original + 1 close
    EXPECT_EQ(segments[0].points[2], segments[0].points[0]);
}

TEST(DecodeLineString, EmptyGeometry) {
    std::vector<uint32_t> geometry;
    auto segments = decode_linestring_geometry(geometry, 4096);
    EXPECT_EQ(segments.size(), 0u);
}

TEST(DecodeLineString, FirstMoveAbsolute) {
    // First MoveTo uses raw coords (not delta from 0,0)
    // Coordinates are zigzag-encoded
    std::vector<uint32_t> geometry = {
        9, 200, 400,  // MoveTo(100,200) — dx=zigzag(200)=100, dy=zigzag(400)=200
        10, 20, 20,   // LineTo(110,210) — dx=zigzag(20)=10, dy=zigzag(20)=10
    };
    auto segments = decode_linestring_geometry(geometry, 4096);
    EXPECT_EQ(segments.size(), 1u);
    EXPECT_EQ(segments[0].points[0], std::make_pair(100, 200));
    EXPECT_EQ(segments[0].points[1], std::make_pair(110, 210));
}

TEST(DecodeLineString, ZigzagCoords) {
    // Verify zigzag decoding: 2 → 1, 1 → -1
    std::vector<uint32_t> geometry = {
        9, 0, 0,      // MoveTo(0,0)
        10, 2, 1,     // LineTo: dx=zigzag(2)=1, dy=zigzag(1)=-1 → (1,-1)
    };
    auto segments = decode_linestring_geometry(geometry, 4096);
    // After MoveTo(0,0), current is (0,0)
    // LineTo with dx=1, dy=-1: new position = (0+1, 0+(-1)) = (1, -1)
    EXPECT_EQ(segments[0].points[1], std::make_pair(1, -1));
}

// ─── 2. Line Extraction ─────────────────────────────────────────────────

TEST(ExtractLines, EmptyTile) {
    Tile tile;
    auto batch = extract_lines(tile, "");
    EXPECT_TRUE(batch.vertices.empty());
    EXPECT_TRUE(batch.indices.empty());
}

TEST(ExtractLines, ClipSpaceMapping) {
    // Create a minimal tile with one line
    Tile tile;
    Layer layer;
    layer.name = "test";
    layer.extent = 4096;

    Feature feat;
    feat.type = GeomType::LINESTRING;
    // Simple 2-point line at (0,0) to (4096,4096)
    // Coordinates are zigzag-encoded: encode(4096) = 8192
    feat.geometry = {
        9, 0, 0,      // MoveTo(0,0)
        10, 8192, 8192, // LineTo(4096,4096) — dx=zigzag(8192)=4096
    };
    layer.features.push_back(std::move(feat));
    tile.layers.push_back(std::move(layer));

    auto batch = extract_lines(tile, "");
    EXPECT_EQ(batch.vertices.size(), 2u);
    // (0,0) → clip (-1, 1)
    EXPECT_NEAR(batch.vertices[0].x, -1.0f, 0.01f);
    EXPECT_NEAR(batch.vertices[0].y, 1.0f, 0.01f);
    // (4096,4096) → clip (1, -1)
    EXPECT_NEAR(batch.vertices[1].x, 1.0f, 0.01f);
    EXPECT_NEAR(batch.vertices[1].y, -1.0f, 0.01f);
}

TEST(ExtractLines, PrimitiveRestart) {
    // Two separate lines should have 0xFFFFFFFF between them
    Tile tile;
    Layer layer;
    layer.name = "test";
    layer.extent = 4096;

    Feature feat1, feat2;
    feat1.type = GeomType::LINESTRING;
    feat1.geometry = { 9, 0, 0, 10, 20, 0 };  // zigzag-encoded coords
    feat2.type = GeomType::LINESTRING;
    feat2.geometry = { 9, 400, 400, 10, 600, 400 };  // zigzag-encoded coords
    layer.features.push_back(std::move(feat1));
    layer.features.push_back(std::move(feat2));
    tile.layers.push_back(std::move(layer));

    auto batch = extract_lines(tile, "");
    // Check for primitive restart marker
    bool found_restart = false;
    for (auto idx : batch.indices) {
        if (idx == 0xFFFFFFFF) {
            found_restart = true;
            break;
        }
    }
    EXPECT_TRUE(found_restart);
}

TEST(ExtractLines, MinimalLine) {
    Tile tile;
    Layer layer;
    layer.name = "test";
    layer.extent = 4096;

    Feature feat;
    feat.type = GeomType::LINESTRING;
    feat.geometry = { 9, 0, 0, 10, 20, 0 };  // zigzag-encoded coords
    layer.features.push_back(std::move(feat));
    tile.layers.push_back(std::move(layer));

    auto batch = extract_lines(tile, "");
    EXPECT_EQ(batch.vertices.size(), 2u);
    EXPECT_GE(batch.indices.size(), 2u);
}

// ─── 3. Polygon Fan Triangulation ───────────────────────────────────────

TEST(FanTriangulate, Triangle) {
    std::vector<std::pair<int32_t, int32_t>> ring = {{0,0}, {100,0}, {50,100}};
    std::vector<PolyVertex> verts;
    std::vector<uint32_t> indices;
    fan_triangulate(ring, 1.0f/4096.0f, verts, indices);
    EXPECT_EQ(verts.size(), 3u);
    EXPECT_EQ(indices.size(), 3u); // 1 triangle
}

TEST(FanTriangulate, Quad) {
    std::vector<std::pair<int32_t, int32_t>> ring = {{0,0}, {100,0}, {100,100}, {0,100}};
    std::vector<PolyVertex> verts;
    std::vector<uint32_t> indices;
    fan_triangulate(ring, 1.0f/4096.0f, verts, indices);
    EXPECT_EQ(verts.size(), 4u);
    EXPECT_EQ(indices.size(), 6u); // 2 triangles
}

TEST(FanTriangulate, Pentagon) {
    std::vector<std::pair<int32_t, int32_t>> ring = {
        {0,0}, {100,0}, {150,50}, {100,100}, {0,100}
    };
    std::vector<PolyVertex> verts;
    std::vector<uint32_t> indices;
    fan_triangulate(ring, 1.0f/4096.0f, verts, indices);
    EXPECT_EQ(verts.size(), 5u);
    EXPECT_EQ(indices.size(), 9u); // 3 triangles
}

TEST(FanTriangulate, TooFewPoints) {
    std::vector<std::pair<int32_t, int32_t>> ring = {{0,0}, {100,0}};
    std::vector<PolyVertex> verts;
    std::vector<uint32_t> indices;
    fan_triangulate(ring, 1.0f/4096.0f, verts, indices);
    EXPECT_TRUE(verts.empty());
    EXPECT_TRUE(indices.empty());
}

TEST(FanTriangulate, ClipSpace) {
    std::vector<std::pair<int32_t, int32_t>> ring = {{0,0}, {4096,0}, {2048,4096}};
    std::vector<PolyVertex> verts;
    std::vector<uint32_t> indices;
    fan_triangulate(ring, 1.0f/4096.0f, verts, indices);
    // (0,0) → clip (-1, 1)
    EXPECT_NEAR(verts[0].x, -1.0f, 0.01f);
    EXPECT_NEAR(verts[0].y, 1.0f, 0.01f);
}

// ─── 4. Polygon Extraction ──────────────────────────────────────────────

TEST(ExtractPolygons, EmptyTile) {
    Tile tile;
    auto batch = extract_polygons(tile, "");
    EXPECT_TRUE(batch.vertices.empty());
    EXPECT_TRUE(batch.indices.empty());
}

TEST(ExtractPolygons, MinimalPolygon) {
    Tile tile;
    Layer layer;
    layer.name = "test";
    layer.extent = 4096;

    Feature feat;
    feat.type = GeomType::POLYGON;
    // Triangle: MoveTo(0,0), LineTo(100,0), LineTo(50,100), ClosePath
    // Coordinates are zigzag-encoded. ClosePath adds first point again.
    feat.geometry = {
        9, 0, 0,        // MoveTo(0,0)
        10, 200, 0,     // LineTo(100,0) — dx=zigzag(200)=100, dy=zigzag(0)=0
        10, 100, 200,   // LineTo(150,100) — dx=zigzag(100)=50, dy=zigzag(200)=100
        15,             // ClosePath
    };
    layer.features.push_back(std::move(feat));
    tile.layers.push_back(std::move(layer));

    auto batch = extract_polygons(tile, "");
    // ClosePath adds first point, so 4 vertices total (3 + 1 close)
    EXPECT_EQ(batch.vertices.size(), 4u);
    // Fan triangulation of 4 points → 2 triangles = 6 indices
    EXPECT_EQ(batch.indices.size(), 6u);
}

TEST(ExtractPolygons, FilterByLayer) {
    Tile tile;
    Layer layer1, layer2;
    layer1.name = "roads";
    layer1.extent = 4096;
    layer2.name = "buildings";
    layer2.extent = 4096;

    Feature line_feat;
    line_feat.type = GeomType::LINESTRING;
    line_feat.geometry = { 9, 0, 0, 10, 20, 0 };  // zigzag-encoded
    layer1.features.push_back(std::move(line_feat));

    Feature poly_feat;
    poly_feat.type = GeomType::POLYGON;
    poly_feat.geometry = { 9, 0, 0, 10, 20, 0, 10, 20, 20, 15 };  // zigzag-encoded
    layer2.features.push_back(std::move(poly_feat));

    tile.layers.push_back(std::move(layer1));
    tile.layers.push_back(std::move(layer2));

    auto batch = extract_polygons(tile, "buildings");
    EXPECT_FALSE(batch.vertices.empty());
    EXPECT_EQ(batch.layer_name, "buildings");
}

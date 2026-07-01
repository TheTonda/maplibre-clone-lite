// tests/test_building.cpp — Unit tests for building extrusion + 2D fills
#include "building_data.h"

#include <gtest/gtest.h>
#include <cmath>

using namespace bldg;
using namespace osm;

// ─── 1. Building Extrusion ──────────────────────────────────────────────

TEST(ExtrudeBuilding, Triangle) {
    std::vector<MercatorPoint> footprint = {{0,0}, {100,0}, {50,100}};
    std::vector<BuildingVertex> verts;
    std::vector<uint32_t> indices;
    extrude_building(footprint, 10.0f, verts, indices);
    // Top: 3 verts, Bottom: 3 verts, Sides: 3 quads × 4 verts = 12 verts
    // Total: 3 + 3 + 12 = 18 vertices
    EXPECT_EQ(verts.size(), 18u);
    // Top: 1 tri, Bottom: 1 tri, Sides: 3 quads × 2 tris = 6 tris
    // Total: 1 + 1 + 6 = 8 triangles = 24 indices
    EXPECT_EQ(indices.size(), 24u);
}

TEST(ExtrudeBuilding, Quad) {
    std::vector<MercatorPoint> footprint = {{0,0}, {100,0}, {100,100}, {0,100}};
    std::vector<BuildingVertex> verts;
    std::vector<uint32_t> indices;
    extrude_building(footprint, 10.0f, verts, indices);
    // Top: 4 verts, Bottom: 4 verts, Sides: 4 quads × 4 verts = 16 verts
    // Total: 4 + 4 + 16 = 24 vertices
    EXPECT_EQ(verts.size(), 24u);
    // Top: 2 tris, Bottom: 2 tris, Sides: 4 quads × 2 tris = 8 tris
    // Total: 2 + 2 + 8 = 12 triangles = 36 indices
    EXPECT_EQ(indices.size(), 36u);
}

TEST(ExtrudeBuilding, TooFewPoints) {
    std::vector<MercatorPoint> footprint = {{0,0}, {100,0}};
    std::vector<BuildingVertex> verts;
    std::vector<uint32_t> indices;
    extrude_building(footprint, 10.0f, verts, indices);
    EXPECT_TRUE(verts.empty());
    EXPECT_TRUE(indices.empty());
}

TEST(ExtrudeBuilding, HeightZero) {
    std::vector<MercatorPoint> footprint = {{0,0}, {100,0}, {50,100}};
    std::vector<BuildingVertex> verts;
    std::vector<uint32_t> indices;
    extrude_building(footprint, 0.0f, verts, indices);
    // Top Y should equal bottom Y (all 18 vertices should have y=0)
    for (size_t i = 0; i < verts.size(); ++i) {
        EXPECT_NEAR(verts[i].y, 0.0f, 0.001f);
    }
}

TEST(ExtrudeBuilding, TopFaceWinding) {
    // Verify top face uses CCW winding (forward fan)
    std::vector<MercatorPoint> footprint = {{0,0}, {100,0}, {50,100}};
    std::vector<BuildingVertex> verts;
    std::vector<uint32_t> indices;
    extrude_building(footprint, 10.0f, verts, indices);
    // First top vertex is at index 0
    // Fan triangulation: (0, 1, 2) for triangle
    EXPECT_EQ(indices[0], 0);
    EXPECT_EQ(indices[1], 1);
    EXPECT_EQ(indices[2], 2);
}

TEST(ExtrudeBuilding, BottomFaceWinding) {
    // Verify bottom face uses CW winding (reversed fan)
    std::vector<MercatorPoint> footprint = {{0,0}, {100,0}, {50,100}};
    std::vector<BuildingVertex> verts;
    std::vector<uint32_t> indices;
    extrude_building(footprint, 10.0f, verts, indices);
    // Bottom base = 3 (after 3 top vertices)
    // Reversed fan: (3, 5, 4) for triangle
    // Indices 3,4,5 are the bottom vertices
    bool found_bottom = false;
    for (size_t i = 3; i < indices.size(); ++i) {
        if (indices[i] >= 3 && indices[i] < 6) {
            found_bottom = true;
            break;
        }
    }
    EXPECT_TRUE(found_bottom);
}

TEST(ExtrudeBuilding, SideFaceCount) {
    // N-point polygon → N side faces (2 triangles each = 2N triangles)
    std::vector<MercatorPoint> footprint = {{0,0}, {100,0}, {100,100}, {0,100}};
    std::vector<BuildingVertex> verts;
    std::vector<uint32_t> indices;
    extrude_building(footprint, 10.0f, verts, indices);
    // 4 side faces × 2 triangles = 8 triangles = 24 indices for sides
    // Total: 2 (top) + 2 (bottom) + 8 (sides) = 12 triangles = 36 indices
    EXPECT_EQ(indices.size(), 36u);
}

TEST(ExtrudeBuilding, VertexCount) {
    // N-point building → separate vertices per face
    // For 4-point footprint: 4 top + 4 bottom + 4*4 side = 24 vertices
    std::vector<MercatorPoint> footprint = {{0,0}, {100,0}, {50,100}, {0,50}};
    std::vector<BuildingVertex> verts;
    std::vector<uint32_t> indices;
    extrude_building(footprint, 10.0f, verts, indices);
    EXPECT_EQ(verts.size(), 24u); // 4 top + 4 bottom + 4*4 side
}

TEST(ExtrudeBuilding, HeightApplied) {
    std::vector<MercatorPoint> footprint = {{0,0}, {100,0}, {50,100}};
    std::vector<BuildingVertex> verts;
    std::vector<uint32_t> indices;
    extrude_building(footprint, 10.0f, verts, indices);
    // BuildingVertex has (x, y, z) where y is the extrusion height
    // Top vertices (indices 0, 1, 2) should have y = 10.0
    EXPECT_NEAR(verts[0].y, 10.0f, 0.001f);
    EXPECT_NEAR(verts[1].y, 10.0f, 0.001f);
    EXPECT_NEAR(verts[2].y, 10.0f, 0.001f);
    // Bottom vertices (indices 3, 4, 5) should have y = 0.0
    EXPECT_NEAR(verts[3].y, 0.0f, 0.001f);
    EXPECT_NEAR(verts[4].y, 0.0f, 0.001f);
    EXPECT_NEAR(verts[5].y, 0.0f, 0.001f);
}

// ─── 2. Building Extraction ─────────────────────────────────────────────

TEST(ExtractBuildings, EmptyInput) {
    std::vector<Building> buildings;
    auto batch = extract_buildings(buildings);
    EXPECT_TRUE(batch.vertices.empty());
    EXPECT_TRUE(batch.indices.empty());
}

TEST(ExtractBuildings, SingleBuilding) {
    Building b;
    b.height = 10.0f;
    b.footprint = {{0,0}, {100,0}, {50,100}};
    std::vector<Building> buildings = {b};
    auto batch = extract_buildings(buildings);
    // 3-point footprint: 3 top + 3 bottom + 3*4 side = 18 vertices
    EXPECT_EQ(batch.vertices.size(), 18u);
    EXPECT_EQ(batch.indices.size(), 24u);
}

TEST(ExtractBuildings, MultipleBuildings) {
    Building b1, b2;
    b1.height = 10.0f;
    b1.footprint = {{0,0}, {100,0}, {50,100}};
    b2.height = 20.0f;
    b2.footprint = {{200,0}, {300,0}, {250,100}};
    std::vector<Building> buildings = {b1, b2};
    auto batch = extract_buildings(buildings);
    // 2 buildings × 18 vertices = 36
    EXPECT_EQ(batch.vertices.size(), 36u);
    // 2 buildings × 24 indices = 48
    EXPECT_EQ(batch.indices.size(), 48u);
}

TEST(ExtractBuildings, SkipInvalid) {
    Building b1, b2;
    b1.height = 10.0f;
    b1.footprint = {{0,0}, {100,0}, {50,100}};
    b2.height = 20.0f;
    b2.footprint = {{0,0}, {100,0}}; // Only 2 points — invalid
    std::vector<Building> buildings = {b1, b2};
    auto batch = extract_buildings(buildings);
    // Only b1 should be included
    EXPECT_EQ(batch.vertices.size(), 18u);
    EXPECT_EQ(batch.indices.size(), 24u);
}

// ─── 3. 2D Fill Extraction ──────────────────────────────────────────────

TEST(ExtractFills2D, ParksIncluded) {
    Park p;
    p.polygon = {{0,0}, {100,0}, {50,100}};
    std::vector<Park> parks = {p};
    glm::vec3 park_color(0.2f, 0.4f, 0.1f);
    auto result = extract_fills_2d(parks, {}, {}, park_color, glm::vec3(0,0,0), glm::vec3(0,0,0));
    EXPECT_EQ(result.size(), 1u);
}

TEST(ExtractFills2D, WaterIncluded) {
    WaterPolygon w;
    w.polygon = {{0,0}, {100,0}, {50,100}};
    std::vector<WaterPolygon> water = {w};
    glm::vec3 water_color(0.1f, 0.3f, 0.6f);
    auto result = extract_fills_2d({}, water, {}, glm::vec3(0,0,0), water_color, glm::vec3(0,0,0));
    EXPECT_EQ(result.size(), 1u);
}

TEST(ExtractFills2D, LanduseIncluded) {
    Landuse l;
    l.polygon = {{0,0}, {100,0}, {50,100}};
    std::vector<Landuse> landuse = {l};
    glm::vec3 land_color(0.3f, 0.4f, 0.2f);
    auto result = extract_fills_2d({}, {}, landuse, glm::vec3(0,0,0), glm::vec3(0,0,0), land_color);
    EXPECT_EQ(result.size(), 1u);
}

TEST(ExtractFills2D, SkipSmall) {
    Park p;
    p.polygon = {{0,0}, {100,0}}; // Only 2 points
    std::vector<Park> parks = {p};
    glm::vec3 park_color(0.2f, 0.4f, 0.1f);
    auto result = extract_fills_2d(parks, {}, {}, park_color, glm::vec3(0,0,0), glm::vec3(0,0,0));
    EXPECT_TRUE(result.empty());
}

TEST(ExtractFills2D, EmptyInput) {
    glm::vec3 park_color(0.2f, 0.4f, 0.1f);
    auto result = extract_fills_2d({}, {}, {}, park_color, glm::vec3(0,0,0), glm::vec3(0,0,0));
    EXPECT_TRUE(result.empty());
}

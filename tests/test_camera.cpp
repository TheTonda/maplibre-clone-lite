// Camera unit tests — Task 13
#include <gtest/gtest.h>
#include <cmath>
#include <unordered_set>
#include <map_renderer/camera.h>
#include <map_renderer/platform.h>

using namespace map_renderer;

// ── Matrix correctness ────────────────────────────────────────────────

TEST(CameraTest, ProjectionMatrixDerivesSpan) {
    Camera c;
    c.set_position(0.0f, 0.0f);
    c.set_visible_span(10000.0f);  // 10km across
    c.clear_dirty();

    // Landscape aspect 2:1 — height spans 10km, width spans 20km
    glm::mat4 proj = c.get_projection_matrix(2.0f);
    // For ortho(left=-10000, right=10000, bottom=-5000, top=5000, near=-1, far=1)
    // Ortho factors: m[0][0] = 2/(r-l), m[1][1] = 2/(t-b)
    EXPECT_NEAR(proj[0][0], 2.0f / 20000.0f, 1e-4f);
    EXPECT_NEAR(proj[1][1], 2.0f / 10000.0f, 1e-4f);
}

TEST(CameraTest, ProjectionMatrixPortrait) {
    Camera c;
    c.set_position(0.0f, 0.0f);
    c.set_visible_span(10000.0f);
    c.clear_dirty();

    // Portrait aspect 0.5 — width spans 10km, height spans 20km
    glm::mat4 proj = c.get_projection_matrix(0.5f);
    EXPECT_NEAR(proj[0][0], 2.0f / 10000.0f, 1e-4f);
    EXPECT_NEAR(proj[1][1], 2.0f / 20000.0f, 1e-4f);
}

TEST(CameraTest, ViewMatrixIsIdentity) {
    Camera c;
    c.clear_dirty();
    glm::mat4 view = c.get_view_matrix();
    EXPECT_FLOAT_EQ(view[0][0], 1.0f);
    EXPECT_FLOAT_EQ(view[1][1], 1.0f);
    EXPECT_FLOAT_EQ(view[2][2], 1.0f);
    EXPECT_FLOAT_EQ(view[3][3], 1.0f);
}

// ── Dirty flag ─────────────────────────────────────────────────────────

TEST(CameraTest, DirtyOnConstruction) {
    Camera c;
    EXPECT_TRUE(c.is_dirty());
}

TEST(CameraTest, ClearDirty) {
    Camera c;
    c.clear_dirty();
    EXPECT_FALSE(c.is_dirty());
}

TEST(CameraTest, MarkDirty) {
    Camera c;
    c.clear_dirty();
    EXPECT_FALSE(c.is_dirty());
    c.mark_dirty();
    EXPECT_TRUE(c.is_dirty());
}

TEST(CameraTest, SetPositionSetsDirty) {
    Camera c;
    c.clear_dirty();
    c.set_position(100.0f, 200.0f);
    EXPECT_TRUE(c.is_dirty());
}

TEST(CameraTest, PanSetsDirty) {
    Camera c;
    c.clear_dirty();
    c.pan(10.0f, 20.0f);
    EXPECT_TRUE(c.is_dirty());
}

TEST(CameraTest, ZoomBySetsDirty) {
    Camera c;
    c.clear_dirty();
    c.zoom_by(2.0f);
    EXPECT_TRUE(c.is_dirty());
}

TEST(CameraTest, ZoomByClamps) {
    Camera c;
    c.set_visible_span(500000.0f);
    c.zoom_by(0.0001f);  // try to zoom way in
    EXPECT_GE(c.get_visible_span(), 100.0f);
    c.set_visible_span(500000.0f);
    c.zoom_by(100.0f);  // try to zoom way out
    EXPECT_LE(c.get_visible_span(), 5000000.0f);
}

// ── Tile zoom selection (LLD §5.2) ────────────────────────────────────

TEST(CameraTest, TileZoomCountry) {
    Camera c;
    c.set_visible_span(600000.0f);  // > 500000
    EXPECT_EQ(c.get_tile_zoom(), 8u);
}

TEST(CameraTest, TileZoomRegion) {
    Camera c;
    c.set_visible_span(60000.0f);  // > 50000
    EXPECT_EQ(c.get_tile_zoom(), 12u);
}

TEST(CameraTest, TileZoomNeighborhood) {
    Camera c;
    c.set_visible_span(6000.0f);  // > 5000
    EXPECT_EQ(c.get_tile_zoom(), 15u);
}

TEST(CameraTest, TileZoomStreet) {
    Camera c;
    c.set_visible_span(3000.0f);  // ≤ 5000
    EXPECT_EQ(c.get_tile_zoom(), 17u);
}

// ── Position getters ────────────────────────────────────────────────────

TEST(CameraTest, GetPosition) {
    Camera c;
    c.set_position(123.0f, 456.0f);
    EXPECT_FLOAT_EQ(c.get_x(), 123.0f);
    EXPECT_FLOAT_EQ(c.get_z(), 456.0f);
}

TEST(CameraTest, GetVisibleSpan) {
    Camera c;
    c.set_visible_span(25000.0f);
    EXPECT_FLOAT_EQ(c.get_visible_span(), 25000.0f);
}

// ── Frame dataset ──────────────────────────────────────────────────────

TEST(CameraTest, FrameDatasetCentersAndFits) {
    Camera c;
    c.set_dataset_bounds(0.0f, 10000.0f, 0.0f, 5000.0f);
    c.frame_dataset();
    EXPECT_FLOAT_EQ(c.get_x(), 5000.0f);
    EXPECT_FLOAT_EQ(c.get_z(), 2500.0f);
    // Span should fit the larger dimension (10000) + 10% margin
    EXPECT_NEAR(c.get_visible_span(), 11000.0f, 1.0f);
    EXPECT_TRUE(c.is_dirty());
}

// ── Visible tiles ──────────────────────────────────────────────────────

TEST(CameraTest, VisibleTilesNonEmpty) {
    Camera c;
    c.set_reference_point(28.589, 77.2375);  // New Delhi center
    c.set_position(0.0f, 0.0f);
    c.set_visible_span(50000.0f);
    auto tiles = c.get_visible_tiles(12);
    EXPECT_FALSE(tiles.empty());
    // All tiles should have zoom 12
    for (const auto& t : tiles) {
        EXPECT_EQ(t.z, 12u);
    }
}

TEST(CameraTest, VisibleTilesClampedToValidRange) {
    Camera c;
    c.set_reference_point(28.589, 77.2375);
    c.set_position(0.0f, 0.0f);
    c.set_visible_span(5000000.0f);  // very wide — covers many tiles
    auto tiles = c.get_visible_tiles(8);
    // 2^8 = 256 tiles per axis, should be within [0, 255]
    for (const auto& t : tiles) {
        EXPECT_LT(t.x, 256u);
        EXPECT_LT(t.y, 256u);
    }
}

TEST(CameraTest, VisibleTilesNoDuplicates) {
    Camera c;
    c.set_reference_point(28.589, 77.2375);
    c.set_position(0.0f, 0.0f);
    c.set_visible_span(50000.0f);
    auto tiles = c.get_visible_tiles(12);
    std::unordered_set<TileId, TileId::Hash> seen;
    for (const auto& t : tiles) {
        auto [it, ok] = seen.insert(t);
        EXPECT_TRUE(ok) << "Duplicate tile in visible_tiles";
    }
}

// ── Input ──────────────────────────────────────────────────────────────

TEST(CameraTest, ApplyInputZoom) {
    Camera c;
    c.set_visible_span(10000.0f);
    c.clear_dirty();
    InputData d{};
    d.type = InputEvent::Zoom;
    d.delta = 1.0f;
    c.apply_input(d, 0.016f);
    EXPECT_NE(c.get_visible_span(), 10000.0f);
    EXPECT_TRUE(c.is_dirty());
}

TEST(CameraTest, ApplyInputPanMove) {
    Camera c;
    c.set_position(0.0f, 0.0f);
    c.set_visible_span(10000.0f);
    c.clear_dirty();
    InputData d{};
    d.type = InputEvent::PanMove;
    d.x = 100.0f;
    d.y = 50.0f;
    c.apply_input(d, 0.016f);
    EXPECT_NE(c.get_x(), 0.0f);
    EXPECT_NE(c.get_z(), 0.0f);
    EXPECT_TRUE(c.is_dirty());
}

TEST(CameraTest, ApplyInputKeyPan) {
    Camera c;
    c.set_position(0.0f, 0.0f);
    c.set_visible_span(10000.0f);
    c.clear_dirty();
    InputData d{};
    d.type = InputEvent::KeyPanRight;
    c.apply_input(d, 0.016f);
    EXPECT_GT(c.get_x(), 0.0f);
    EXPECT_TRUE(c.is_dirty());
}
#include "viewport.h"

#include <gtest/gtest.h>

#include <cmath>

using maprender::Viewport;
using maprender::world_x_to_lon;
using maprender::world_y_to_lat;
using maprender::lon_to_world_x;
using maprender::lat_to_world_y;
using maprender::world_width_at;

TEST(Viewport, RoundTripLonLat) {
    for (double lon : {-180.0, -90.0, 0.0, 45.0, 90.0, 180.0}) {
        for (double lat : {-60.0, -10.0, 0.0, 30.0, 60.0}) {
            const int z = 8;
            const double x = lon_to_world_x(lon, z);
            const double y = lat_to_world_y(lat, z);
            EXPECT_NEAR(world_x_to_lon(x, z), lon, 1e-6) << "lon=" << lon;
            EXPECT_NEAR(world_y_to_lat(y, z), lat, 1e-6) << "lat=" << lat;
        }
    }
}

TEST(Viewport, WorldWidthDoubles) {
    for (int z = 0; z <= 20; ++z) {
        EXPECT_DOUBLE_EQ(world_width_at(z), 256.0 * (1ull << z));
    }
}

TEST(Viewport, WorldWidthAtHighZoom) {
    EXPECT_DOUBLE_EQ(world_width_at(18), 256.0 * (1ull << 18));
    EXPECT_DOUBLE_EQ(world_width_at(20), 256.0 * (1ull << 20));
}

TEST(Viewport, TileRangeBasic) {
    Viewport vp;
    vp.set_view(0.0, 0.0, 8, 512, 512);
    // At z=8 the world is 256x256 = 65536 px. Center (0,0) world px = (32768, 32768).
    EXPECT_DOUBLE_EQ(vp.center_x, 32768.0);
    EXPECT_DOUBLE_EQ(vp.center_y, 32768.0);
    EXPECT_EQ(vp.tile_x_min(), 127);
    EXPECT_EQ(vp.tile_x_max(), 128);
    EXPECT_EQ(vp.tile_y_min(), 127);
    EXPECT_EQ(vp.tile_y_max(), 128);
}

TEST(Viewport, TmsSlippyFlipRoundTrip) {
    for (int z : {0, 8, 17, 18, 20}) {
        const int max_row = static_cast<int>((1u << z) - 1);
        for (int slippy_y : {0, max_row / 2, max_row}) {
            const int tms_y = max_row - slippy_y;
            EXPECT_EQ(max_row - tms_y, slippy_y) << "z=" << z << " y=" << slippy_y;
        }
    }
}

TEST(Viewport, TileRangeAtZ18) {
    Viewport vp;
    vp.set_view(77.23, 28.63, 18, 1024, 768);
    EXPECT_EQ(vp.tile_x_min(), 187307);
    EXPECT_EQ(vp.tile_x_max(), 187311);
    EXPECT_EQ(vp.tile_y_min(), 109296);
    EXPECT_EQ(vp.tile_y_max(), 109299);
}

TEST(Viewport, PanShiftsCenter) {
    Viewport vp;
    vp.set_view(0.0, 0.0, 4, 256, 256);
    const double cx0 = vp.center_x;
    vp.pan(16, -8);
    EXPECT_NEAR(vp.center_x, cx0 - 16, 1e-9);
    EXPECT_NEAR(vp.center_y, vp.center_y, 0);
}

TEST(Viewport, ZoomAnchorPreservesAnchorPixel) {
    Viewport vp;
    vp.set_view(10.0, 50.0, 8, 512, 512);
    const double anchor_lon = 10.5;
    const double anchor_lat = 50.5;
    const double ax0 = lon_to_world_x(anchor_lon, 8);
    const double ay0 = lat_to_world_y(anchor_lat, 8);
    const double screen_x0 = ax0 - vp.center_x;  // px offset from screen center
    const double screen_y0 = ay0 - vp.center_y;

    ASSERT_TRUE(vp.zoom_by(1, anchor_lon, anchor_lat));
    const double ax1 = lon_to_world_x(anchor_lon, vp.zoom);
    const double ay1 = lat_to_world_y(anchor_lat, vp.zoom);
    EXPECT_NEAR(ax1 - vp.center_x, screen_x0, 1e-3);
    EXPECT_NEAR(ay1 - vp.center_y, screen_y0, 1e-3);
}

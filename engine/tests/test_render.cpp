#include "maprender/c_api.h"

#include <gtest/gtest.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
const char* kFixture = "engine/tests/fixtures/sample.webp.mbtiles";

std::string out_path(const char* name) {
    return std::string("engine/tests/snapshots/") + name;
}
}  // namespace

TEST(Render, NewDelhiCenterAtZ8) {
    MR_Context* ctx = mr_open(kFixture);
    ASSERT_NE(ctx, nullptr) << mr_last_error(nullptr);
    // Center exactly on the corner between the four fixture tiles so a
    // 256x256 viewport is fully covered by them.
    const double lon = ((182 + 184) / 2.0) / 256.0 * 360.0 - 180.0;  // ~78.05
    const double lat = 28.54;  // ~world_y 27392 (corner)
    mr_set_view(ctx, lon, lat, 8, 256, 256);
    const MR_Frame* f = mr_render(ctx);
    ASSERT_NE(f, nullptr);
    ASSERT_EQ(mr_frame_width(f), 256);
    ASSERT_EQ(mr_frame_height(f), 256);
    const unsigned char* px = mr_frame_pixels(f);
    // Background fill is (0xee,0xee,0xee). At least one tile should paint
    // non-bg pixels; with the corner-centered 256x256 view the whole frame
    // should be tile pixels.
    size_t non_bg = 0;
    for (size_t i = 0; i < 256 * 256; ++i) {
        const unsigned char r = px[i*4], g = px[i*4+1], b = px[i*4+2];
        if (r != 0xee || g != 0xee || b != 0xee) ++non_bg;
    }
    EXPECT_GT(non_bg, 0u);

    stbi_write_png(out_path("render_center.png").c_str(), 256, 256, 4, px, 256 * 4);
    mr_close(ctx);
}

TEST(Render, PanChangesPixels) {
    MR_Context* ctx = mr_open(kFixture);
    ASSERT_NE(ctx, nullptr);
    mr_set_view(ctx, 78.05, 28.54, 8, 256, 256);
    const MR_Frame* f0 = mr_render(ctx);
    ASSERT_NE(f0, nullptr);
    std::vector<unsigned char> before(mr_frame_pixels(f0),
                                       mr_frame_pixels(f0) + 256 * 256 * 4);
    mr_pan(ctx, 40, 0);
    const MR_Frame* f2 = mr_render(ctx);
    ASSERT_NE(f2, nullptr);
    bool any_diff = false;
    for (size_t i = 0; i < before.size(); ++i) {
        if (before[i] != mr_frame_pixels(f2)[i]) { any_diff = true; break; }
    }
    EXPECT_TRUE(any_diff);
    mr_close(ctx);
}

TEST(Render, ZoomAnchorsAndClamps) {
    MR_Context* ctx = mr_open(kFixture);
    ASSERT_NE(ctx, nullptr);
    // Fixture has only z=8, so zooming should clamp.
    mr_set_view(ctx, 77.21, 28.58, 8, 256, 256);
    mr_zoom(ctx, +1, 77.5, 28.7);  // should clamp to z=8
    EXPECT_EQ(mr_max_zoom(ctx), 8);
    mr_close(ctx);
}
#include <gtest/gtest.h>
#include <map_renderer/color_table.h>

TEST(ColorTableTest, KnownColors) {
    using map_renderer::get_color;
    using map_renderer::Color;

    auto ground = get_color("ground");
    EXPECT_FLOAT_EQ(ground.r, 0.12f);
    EXPECT_FLOAT_EQ(ground.g, 0.12f);
    EXPECT_FLOAT_EQ(ground.b, 0.14f);
    EXPECT_FLOAT_EQ(ground.a, 1.0f);

    auto water = get_color("water");
    EXPECT_FLOAT_EQ(water.r, 0.30f);
    EXPECT_FLOAT_EQ(water.g, 0.55f);
    EXPECT_FLOAT_EQ(water.b, 0.79f);

    auto park = get_color("park");
    EXPECT_FLOAT_EQ(park.r, 0.56f);
    EXPECT_FLOAT_EQ(park.g, 0.73f);
    EXPECT_FLOAT_EQ(park.b, 0.50f);

    auto building = get_color("building");
    EXPECT_FLOAT_EQ(building.r, 0.85f);

    auto road = get_color("road");
    EXPECT_FLOAT_EQ(road.r, 0.95f);

    auto primary = get_color("road_primary");
    EXPECT_FLOAT_EQ(primary.r, 1.00f);
    EXPECT_FLOAT_EQ(primary.g, 0.98f);
    EXPECT_FLOAT_EQ(primary.b, 0.90f);

    auto secondary = get_color("road_secondary");
    EXPECT_FLOAT_EQ(secondary.r, 0.94f);

    auto landuse = get_color("landuse");
    EXPECT_FLOAT_EQ(landuse.r, 0.91f);
    EXPECT_FLOAT_EQ(landuse.g, 0.88f);
    EXPECT_FLOAT_EQ(landuse.b, 0.85f);
}

TEST(ColorTableTest, UnknownReturnsMagenta) {
    auto c = map_renderer::get_color("nonexistent_type");
    EXPECT_FLOAT_EQ(c.r, 1.0f);
    EXPECT_FLOAT_EQ(c.g, 0.0f);
    EXPECT_FLOAT_EQ(c.b, 1.0f);
    EXPECT_FLOAT_EQ(c.a, 1.0f);
}

TEST(ColorTableTest, AllColorsHaveAlphaOne) {
    // All predefined colors should have full alpha
    for (const auto& type : {"ground", "water", "park", "landuse",
                             "building", "road", "road_primary", "road_secondary"}) {
        auto c = map_renderer::get_color(type);
        EXPECT_FLOAT_EQ(c.a, 1.0f);
    }
}

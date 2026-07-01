#include <gtest/gtest.h>
#include "data/style_engine.h"

TEST(StyleEngineTest, DefaultStyle) {
    auto engine = StyleEngine::default_style();
    auto rule = engine.get_rule("building");
    EXPECT_FLOAT_EQ(rule.fill_color.r, 0.851f);
    EXPECT_FLOAT_EQ(rule.fill_color.g, 0.765f);
    EXPECT_FLOAT_EQ(rule.fill_color.b, 0.647f);
}

TEST(StyleEngineTest, UnknownTypeFallsBackToGround) {
    auto engine = StyleEngine::default_style();
    auto rule = engine.get_rule("nonexistent_feature");
    // Falls back to ground which is the last default
    EXPECT_FLOAT_EQ(rule.fill_color.r, 0.118f);
}

TEST(StyleEngineTest, LoadFromJson) {
    const std::string json = R"([
        {"type": "building", "fill_color": [1.0, 0.5, 0.0, 1.0], "fill_extrusion_height": 2.0},
        {"type": "water", "fill_color": [0.0, 0.0, 1.0]}
    ])";

    StyleEngine engine;
    engine.load_from_json(json);

    auto b = engine.get_rule("building");
    EXPECT_FLOAT_EQ(b.fill_color.r, 1.0f);
    EXPECT_FLOAT_EQ(b.fill_color.g, 0.5f);
    EXPECT_FLOAT_EQ(b.fill_extrusion_height, 2.0f);

    auto w = engine.get_rule("water");
    EXPECT_FLOAT_EQ(w.fill_color.b, 1.0f);
}

TEST(StyleEngineTest, InvalidJsonFallsBack) {
    const std::string bad_json = "this is not json";
    StyleEngine engine;
    engine.load_from_json(bad_json);
    // Should fall back to defaults without crashing
    auto rule = engine.get_rule("building");
    EXPECT_FLOAT_EQ(rule.fill_color.r, 0.851f);
}

TEST(StyleEngineTest, RoadDefaults) {
    auto engine = StyleEngine::default_style();
    auto rp = engine.get_rule("road_primary");
    EXPECT_FLOAT_EQ(rp.line_width_meters, 8.0f);
    EXPECT_FLOAT_EQ(rp.line_color.r, 1.0f);

    auto rs = engine.get_rule("road_service");
    EXPECT_FLOAT_EQ(rs.line_width_meters, 3.0f);
}

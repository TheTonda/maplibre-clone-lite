// tests/test_style.cpp — Unit tests for StyleEngine
#include "style_engine.h"

#include <gtest/gtest.h>
#include <fstream>
#include <cstdio>

using namespace style;

// ─── 1. Hex Color Parsing ────────────────────────────────────────────────

TEST(HexColor, ValidRed) {
    auto c = parse_hex_color("#ff0000");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR((*c)[0], 1.0f, 0.001f);
    EXPECT_NEAR((*c)[1], 0.0f, 0.001f);
    EXPECT_NEAR((*c)[2], 0.0f, 0.001f);
}

TEST(HexColor, ValidWhite) {
    auto c = parse_hex_color("#ffffff");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR((*c)[0], 1.0f, 0.001f);
    EXPECT_NEAR((*c)[1], 1.0f, 0.001f);
    EXPECT_NEAR((*c)[2], 1.0f, 0.001f);
}

TEST(HexColor, ValidBlack) {
    auto c = parse_hex_color("#000000");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR((*c)[0], 0.0f, 0.001f);
    EXPECT_NEAR((*c)[1], 0.0f, 0.001f);
    EXPECT_NEAR((*c)[2], 0.0f, 0.001f);
}

TEST(HexColor, ValidMixed) {
    auto c = parse_hex_color("#aabbcc");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR((*c)[0], 170.0f / 255.0f, 0.01f);
    EXPECT_NEAR((*c)[1], 187.0f / 255.0f, 0.01f);
    EXPECT_NEAR((*c)[2], 204.0f / 255.0f, 0.01f);
}

TEST(HexColor, TooShort) {
    auto c = parse_hex_color("#fff");
    EXPECT_FALSE(c.has_value());
}

TEST(HexColor, NoHash) {
    auto c = parse_hex_color("ff0000");
    EXPECT_FALSE(c.has_value());
}

TEST(HexColor, InvalidChars) {
    auto c = parse_hex_color("#gggggg");
    EXPECT_FALSE(c.has_value());
}

// ─── 2. Style Loading ───────────────────────────────────────────────────

TEST(StyleEngine, LoadValidJson) {
    StyleEngine engine;
    ASSERT_TRUE(engine.loadFromJson("data/style.json"));
    EXPECT_EQ(engine.layers().size(), 9u);
}

TEST(StyleEngine, LoadMissingFile) {
    StyleEngine engine;
    EXPECT_FALSE(engine.loadFromJson("nonexistent.json"));
}

TEST(StyleEngine, LoadEmptyJson) {
    // Write temporary empty JSON
    const char* path = "/tmp/test_empty_style.json";
    {
        std::ofstream f(path);
        f << "{}";
    }
    StyleEngine engine;
    ASSERT_TRUE(engine.loadFromJson(path));
    EXPECT_EQ(engine.layers().size(), 0u);
    std::remove(path);
}

TEST(StyleEngine, LoadMinimalLayer) {
    const char* path = "/tmp/test_minimal_style.json";
    {
        std::ofstream f(path);
        f << "{\"layers\":[{\"id\":\"x\",\"type\":\"fill\",\"paint\":{}}]}";
    }
    StyleEngine engine;
    ASSERT_TRUE(engine.loadFromJson(path));
    EXPECT_EQ(engine.layers().size(), 1u);
    EXPECT_EQ(engine.layers()[0].id, "x");
    EXPECT_EQ(engine.layers()[0].type, "fill");
    std::remove(path);
}

// ─── 3. Rule Matching ───────────────────────────────────────────────────

TEST(StyleEngine, MatchExistingLayerFill) {
    StyleEngine engine;
    ASSERT_TRUE(engine.loadFromJson("data/style.json"));
    auto rule = engine.matchRule("buildings", "fill");
    // Should match the buildings fill-extrusion layer or default
    // The exact color depends on style.json — verify it's a valid color
    EXPECT_GE(rule.fill_color[0], 0.0f);
    EXPECT_LE(rule.fill_color[0], 1.0f);
}

TEST(StyleEngine, MatchExistingLayerLine) {
    StyleEngine engine;
    ASSERT_TRUE(engine.loadFromJson("data/style.json"));
    auto rule = engine.matchRule("streets", "line");
    EXPECT_GE(rule.line_color[0], 0.0f);
    EXPECT_LE(rule.line_color[0], 1.0f);
}

TEST(StyleEngine, MatchMissingLayer) {
    StyleEngine engine;
    ASSERT_TRUE(engine.loadFromJson("data/style.json"));
    auto rule = engine.matchRule("nonexistent", "fill");
    // Should return default gray
    EXPECT_NEAR(rule.fill_color[0], 0.5f, 0.01f);
    EXPECT_NEAR(rule.fill_color[1], 0.5f, 0.01f);
    EXPECT_NEAR(rule.fill_color[2], 0.5f, 0.01f);
}

TEST(StyleEngine, MatchWildcardLayer) {
    StyleEngine engine;
    ASSERT_TRUE(engine.loadFromJson("data/style.json"));
    auto rule = engine.matchRule("*", "fill");
    // Should match the first fill layer (background or triangle)
    EXPECT_GE(rule.fill_color[0], 0.0f);
    EXPECT_LE(rule.fill_color[0], 1.0f);
}

TEST(StyleEngine, MatchFillExtrusionType) {
    StyleEngine engine;
    ASSERT_TRUE(engine.loadFromJson("data/style.json"));
    auto rule = engine.matchRule("buildings", "fill-extrusion");
    // Should return extrude color from buildings layer
    EXPECT_GE(rule.extrude_color[0], 0.0f);
    EXPECT_LE(rule.extrude_color[0], 1.0f);
    EXPECT_NEAR(rule.extrude_opacity, 0.92f, 0.01f);
}

TEST(StyleEngine, MatchByMvtGeomPoint) {
    StyleEngine engine;
    ASSERT_TRUE(engine.loadFromJson("data/style.json"));
    auto rule = engine.matchRule("parks", 1); // POINT
    // POINT maps to "symbol" type
    // No symbol layer in style.json, so should return default
    EXPECT_NEAR(rule.fill_color[0], 0.5f, 0.01f);
}

TEST(StyleEngine, MatchByMvtGeomLine) {
    StyleEngine engine;
    ASSERT_TRUE(engine.loadFromJson("data/style.json"));
    auto rule = engine.matchRule("streets", 2); // LINESTRING
    // LINESTRING maps to "line" type
    auto rule2 = engine.matchRule("streets", "line");
    // Should match same rule
    EXPECT_EQ(rule.line_color[0], rule2.line_color[0]);
}

TEST(StyleEngine, MatchByMvtGeomPolygon) {
    StyleEngine engine;
    ASSERT_TRUE(engine.loadFromJson("data/style.json"));
    auto rule = engine.matchRule("buildings", 3); // POLYGON
    // POLYGON maps to "fill" type
    auto rule2 = engine.matchRule("buildings", "fill");
    // Should match same rule
    EXPECT_EQ(rule.fill_color[0], rule2.fill_color[0]);
}

TEST(StyleEngine, LayerColorsCorrect) {
    StyleEngine engine;
    ASSERT_TRUE(engine.loadFromJson("data/style.json"));
    // Verify known colors from style.json
    auto bg = engine.matchRule("background", "background");
    // #1a1a2e → approximately [0.102, 0.102, 0.180]
    EXPECT_NEAR(bg.fill_color[0], 26.0f/255.0f, 0.01f);
    EXPECT_NEAR(bg.fill_color[1], 26.0f/255.0f, 0.01f);
    EXPECT_NEAR(bg.fill_color[2], 46.0f/255.0f, 0.01f);

    auto tri = engine.matchRule("triangle", "fill");
    // #ff6600 → [1.0, 0.4, 0.0]
    EXPECT_NEAR(tri.fill_color[0], 255.0f/255.0f, 0.01f);
    EXPECT_NEAR(tri.fill_color[1], 102.0f/255.0f, 0.01f);
    EXPECT_NEAR(tri.fill_color[2], 0.0f, 0.01f);
}

TEST(StyleEngine, LayerCountCorrect) {
    StyleEngine engine;
    ASSERT_TRUE(engine.loadFromJson("data/style.json"));
    EXPECT_EQ(engine.layers().size(), 9u);
}

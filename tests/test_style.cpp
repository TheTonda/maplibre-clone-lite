// tests/test_style.cpp — Unit tests for StyleEngine
#include <gtest/gtest.h>
#include "style_engine.h"
#include <fstream>
#include <cstdio>

using namespace style;

// ─── Hex Color Parsing ───────────────────────────────────────────────

TEST(StyleEngine, ParseHexColorValid) {
    auto c = parse_hex_color("#ff0000");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR((*c)[0], 1.0f, 0.01f);
    EXPECT_NEAR((*c)[1], 0.0f, 0.01f);
    EXPECT_NEAR((*c)[2], 0.0f, 0.01f);
}

TEST(StyleEngine, ParseHexColorWhite) {
    auto c = parse_hex_color("#ffffff");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR((*c)[0], 1.0f, 0.01f);
    EXPECT_NEAR((*c)[1], 1.0f, 0.01f);
    EXPECT_NEAR((*c)[2], 1.0f, 0.01f);
}

TEST(StyleEngine, ParseHexColorBlack) {
    auto c = parse_hex_color("#000000");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR((*c)[0], 0.0f, 0.01f);
    EXPECT_NEAR((*c)[1], 0.0f, 0.01f);
    EXPECT_NEAR((*c)[2], 0.0f, 0.01f);
}

TEST(StyleEngine, ParseHexColorMixed) {
    auto c = parse_hex_color("#aabbcc");
    ASSERT_TRUE(c.has_value());
    EXPECT_NEAR((*c)[0], 0.667f, 0.01f);
    EXPECT_NEAR((*c)[1], 0.729f, 0.01f);
    EXPECT_NEAR((*c)[2], 0.8f, 0.01f);
}

TEST(StyleEngine, ParseHexColorTooShort) {
    auto c = parse_hex_color("#fff");
    EXPECT_FALSE(c.has_value());
}

TEST(StyleEngine, ParseHexColorNoHash) {
    auto c = parse_hex_color("ff0000");
    EXPECT_FALSE(c.has_value());
}

TEST(StyleEngine, ParseHexColorInvalid) {
    auto c = parse_hex_color("#gggggg");
    EXPECT_FALSE(c.has_value());
}

// ─── Style Loading ───────────────────────────────────────────────────

TEST(StyleEngine, LoadValidJson) {
    StyleEngine engine;
    bool ok = engine.loadFromJson("data/style.json");
    EXPECT_TRUE(ok);
    EXPECT_EQ(engine.layers().size(), 9u);
}

TEST(StyleEngine, LoadMissingFile) {
    StyleEngine engine;
    bool ok = engine.loadFromJson("nonexistent.json");
    EXPECT_FALSE(ok);
}

TEST(StyleEngine, LoadEmptyJson) {
    // Create temporary empty JSON
    std::string path = "/tmp/test_empty_style.json";
    {
        std::ofstream f(path);
        f << "{}";
    }
    StyleEngine engine;
    bool ok = engine.loadFromJson(path);
    EXPECT_TRUE(ok);
    EXPECT_EQ(engine.layers().size(), 0u);
    std::remove(path.c_str());
}

TEST(StyleEngine, LoadMinimalLayer) {
    std::string path = "/tmp/test_minimal_style.json";
    {
        std::ofstream f(path);
        f << "{\"layers\":[{\"id\":\"x\",\"type\":\"fill\",\"paint\":{}}]}";
    }
    StyleEngine engine;
    bool ok = engine.loadFromJson(path);
    EXPECT_TRUE(ok);
    EXPECT_EQ(engine.layers().size(), 1u);
    EXPECT_EQ(engine.layers()[0].id, "x");
    EXPECT_EQ(engine.layers()[0].type, "fill");
    std::remove(path.c_str());
}

// ─── Rule Matching ───────────────────────────────────────────────────

TEST(StyleEngine, MatchExistingLayerFill) {
    StyleEngine engine;
    engine.loadFromJson("data/style.json");
    auto rule = engine.matchRule("buildings", "fill");
    // Should match the buildings layer (type: fill-extrusion)
    EXPECT_NEAR(rule.fill_color[0], 0.533f, 0.02f); // #888888
    EXPECT_NEAR(rule.fill_color[1], 0.533f, 0.02f);
    EXPECT_NEAR(rule.fill_color[2], 0.533f, 0.02f);
}

TEST(StyleEngine, MatchExistingLayerLine) {
    StyleEngine engine;
    engine.loadFromJson("data/style.json");
    auto rule = engine.matchRule("streets", "line");
    // Should match the streets layer
    EXPECT_NEAR(rule.line_color[0], 0.933f, 0.02f); // #eedd88
    EXPECT_NEAR(rule.line_color[1], 0.867f, 0.02f);
    EXPECT_NEAR(rule.line_color[2], 0.533f, 0.02f);
}

TEST(StyleEngine, MatchMissingLayer) {
    StyleEngine engine;
    engine.loadFromJson("data/style.json");
    auto rule = engine.matchRule("nonexistent", "fill");
    // Should return default gray rule
    EXPECT_NEAR(rule.fill_color[0], 0.5f, 0.01f);
    EXPECT_NEAR(rule.fill_color[1], 0.5f, 0.01f);
    EXPECT_NEAR(rule.fill_color[2], 0.5f, 0.01f);
}

TEST(StyleEngine, MatchFillExtrusionType) {
    StyleEngine engine;
    engine.loadFromJson("data/style.json");
    auto rule = engine.matchRule("buildings", "fill-extrusion");
    EXPECT_NEAR(rule.extrude_color[0], 0.533f, 0.02f); // #888888
    EXPECT_NEAR(rule.extrude_opacity, 0.92f, 0.01f);
}

TEST(StyleEngine, MatchByMvtGeomPoint) {
    StyleEngine engine;
    engine.loadFromJson("data/style.json");
    auto rule = engine.matchRule("parks", 1); // POINT → symbol
    // Should find a symbol-style rule or default
    EXPECT_TRUE(true); // Just verify no crash
}

TEST(StyleEngine, MatchByMvtGeomLine) {
    StyleEngine engine;
    engine.loadFromJson("data/style.json");
    auto rule = engine.matchRule("streets", 2); // LINESTRING → line
    EXPECT_NEAR(rule.line_color[0], 0.933f, 0.02f);
}

TEST(StyleEngine, MatchByMvtGeomPolygon) {
    StyleEngine engine;
    engine.loadFromJson("data/style.json");
    auto rule = engine.matchRule("buildings", 3); // POLYGON → fill
    EXPECT_NEAR(rule.fill_color[0], 0.533f, 0.02f);
}

TEST(StyleEngine, LayerColorsCorrect) {
    StyleEngine engine;
    engine.loadFromJson("data/style.json");
    auto bg_rule = engine.matchRule("background", "background");
    EXPECT_NEAR(bg_rule.fill_color[0], 0.102f, 0.02f); // #1a1a2e
    EXPECT_NEAR(bg_rule.fill_color[1], 0.102f, 0.02f);
    EXPECT_NEAR(bg_rule.fill_color[2], 0.18f, 0.02f);
}

TEST(StyleEngine, LayerCountCorrect) {
    StyleEngine engine;
    engine.loadFromJson("data/style.json");
    EXPECT_EQ(engine.layers().size(), 9u);
}

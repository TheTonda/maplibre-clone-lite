// tests/test_json.cpp — Unit tests for JSON parser (json namespace in osm_loader.h)
#include <gtest/gtest.h>
#include "osm_loader.h"

using namespace osm::json;

// ─── Tokenization & Parsing ──────────────────────────────────────────

TEST(JsonParser, ParseEmptyObject) {
    const char* input = "{}";
    const char* p = input;
    auto v = parse_value(p);
    EXPECT_EQ(v.kind, JsonKind::Object);
    EXPECT_EQ(v.size(), 0u);
}

TEST(JsonParser, ParseEmptyArray) {
    const char* input = "[]";
    const char* p = input;
    auto v = parse_value(p);
    EXPECT_EQ(v.kind, JsonKind::Array);
    EXPECT_EQ(v.size(), 0u);
}

TEST(JsonParser, ParseStringEscaping) {
    const char* input = "\"hello\\nworld\\t\\\\\\\"\"";
    const char* p = input;
    auto v = parse_value(p);
    EXPECT_EQ(v.kind, JsonKind::String);
    EXPECT_EQ(v.sval, "hello\nworld\t\\\"");
}

TEST(JsonParser, ParseNumberInteger) {
    const char* input = "42";
    const char* p = input;
    auto v = parse_value(p);
    EXPECT_EQ(v.kind, JsonKind::Number);
    EXPECT_DOUBLE_EQ(v.num, 42.0);
}

TEST(JsonParser, ParseNumberNegative) {
    const char* input = "-7.5";
    const char* p = input;
    auto v = parse_value(p);
    EXPECT_EQ(v.kind, JsonKind::Number);
    EXPECT_DOUBLE_EQ(v.num, -7.5);
}

TEST(JsonParser, ParseNumberScientific) {
    const char* input = "1.5e-3";
    const char* p = input;
    auto v = parse_value(p);
    EXPECT_EQ(v.kind, JsonKind::Number);
    EXPECT_NEAR(v.num, 0.0015, 1e-10);
}

TEST(JsonParser, ParseBooleanTrue) {
    const char* input = "true";
    const char* p = input;
    auto v = parse_value(p);
    EXPECT_EQ(v.kind, JsonKind::Bool);
    EXPECT_TRUE(v.bval);
}

TEST(JsonParser, ParseBooleanFalse) {
    const char* input = "false";
    const char* p = input;
    auto v = parse_value(p);
    EXPECT_EQ(v.kind, JsonKind::Bool);
    EXPECT_FALSE(v.bval);
}

TEST(JsonParser, ParseNull) {
    const char* input = "null";
    const char* p = input;
    auto v = parse_value(p);
    EXPECT_EQ(v.kind, JsonKind::Null);
}

TEST(JsonParser, ParseNestedObject) {
    const char* input = "{\"a\":{\"b\":1}}";
    const char* p = input;
    auto v = parse_value(p);
    EXPECT_EQ(v.kind, JsonKind::Object);
    EXPECT_EQ(v.size(), 1u);
    auto* inner = v.find("a");
    ASSERT_NE(inner, nullptr);
    EXPECT_EQ(inner->kind, JsonKind::Object);
    EXPECT_EQ(inner->size(), 1u);
    auto* b = inner->find("b");
    ASSERT_NE(b, nullptr);
    EXPECT_DOUBLE_EQ(b->num, 1.0);
}

TEST(JsonParser, ParseMixedArray) {
    const char* input = "[1, \"two\", true]";
    const char* p = input;
    auto v = parse_value(p);
    EXPECT_EQ(v.kind, JsonKind::Array);
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ((*v[0]).kind, JsonKind::Number);
    EXPECT_EQ((*v[1]).kind, JsonKind::String);
    EXPECT_EQ((*v[2]).kind, JsonKind::Bool);
}

TEST(JsonParser, ParseSkipWhitespace) {
    const char* input = "  { \"a\" : 1 }  ";
    const char* p = input;
    auto v = parse_value(p);
    EXPECT_EQ(v.kind, JsonKind::Object);
    EXPECT_EQ(v.size(), 1u);
}

// ─── JsonValue API ───────────────────────────────────────────────────

TEST(JsonValue, FindExistingKey) {
    const char* input = "{\"key\": 42}";
    const char* p = input;
    auto v = parse_value(p);
    auto* found = v.find("key");
    ASSERT_NE(found, nullptr);
    EXPECT_DOUBLE_EQ(found->num, 42.0);
}

TEST(JsonValue, FindMissingKey) {
    const char* input = "{\"key\": 42}";
    const char* p = input;
    auto v = parse_value(p);
    auto* found = v.find("missing");
    EXPECT_EQ(found, nullptr);
}

TEST(JsonValue, FindNonObject) {
    const char* input = "42";
    const char* p = input;
    auto v = parse_value(p);
    auto* found = v.find("key");
    EXPECT_EQ(found, nullptr);
}

TEST(JsonValue, SizeArray) {
    const char* input = "[1, 2, 3]";
    const char* p = input;
    auto v = parse_value(p);
    EXPECT_EQ(v.size(), 3u);
}

TEST(JsonValue, SizeObject) {
    const char* input = "{\"a\":1,\"b\":2}";
    const char* p = input;
    auto v = parse_value(p);
    EXPECT_EQ(v.size(), 2u);
}

TEST(JsonValue, SizeNonContainer) {
    const char* input = "42";
    const char* p = input;
    auto v = parse_value(p);
    EXPECT_EQ(v.size(), 0u);
}

TEST(JsonValue, AsNumberDefault) {
    const char* input = "{\"key\": 42}";
    const char* p = input;
    auto v = parse_value(p);
    auto* missing = v.find("missing");
    EXPECT_EQ(missing->as_number(99.0), 99.0);
}

TEST(JsonValue, AsStringDefault) {
    const char* input = "{\"key\": 42}";
    const char* p = input;
    auto v = parse_value(p);
    auto* missing = v.find("missing");
    EXPECT_EQ(missing->as_string("default"), "default");
}

TEST(JsonValue, AsBoolDefault) {
    const char* input = "{\"key\": 42}";
    const char* p = input;
    auto v = parse_value(p);
    auto* missing = v.find("missing");
    EXPECT_EQ(missing->as_bool(true), true);
}

TEST(JsonValue, OperatorIndexArray) {
    const char* input = "[10, 20, 30]";
    const char* p = input;
    auto v = parse_value(p);
    EXPECT_DOUBLE_EQ((*v[0]).num, 10.0);
    EXPECT_DOUBLE_EQ((*v[1]).num, 20.0);
    EXPECT_DOUBLE_EQ((*v[2]).num, 30.0);
}

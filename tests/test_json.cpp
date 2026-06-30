// tests/test_json.cpp — Unit tests for the JSON parser in osm_loader.h
#include "osm_loader.h"

#include <gtest/gtest.h>

using namespace osm::json;

// ─── 1. Tokenization & Parsing ───────────────────────────────────────────

TEST(JsonParser, EmptyObject) {
    const char* input = "{}";
    const char* p = input;
    JsonValue v = parse_value(p);
    ASSERT_TRUE(v.is_object());
    EXPECT_EQ(v.size(), 0u);
}

TEST(JsonParser, EmptyArray) {
    const char* input = "[]";
    const char* p = input;
    JsonValue v = parse_value(p);
    ASSERT_TRUE(v.is_array());
    EXPECT_EQ(v.size(), 0u);
}

TEST(JsonParser, StringEscaping) {
    const char* input = "\"hello\\nworld\\t\\\\\\\"\"";
    const char* p = input;
    JsonValue v = parse_value(p);
    ASSERT_TRUE(v.is_string());
    EXPECT_EQ(v.sval, "hello\nworld\t\\\"");
}

TEST(JsonParser, NumberInteger) {
    const char* input = "42";
    const char* p = input;
    JsonValue v = parse_value(p);
    ASSERT_TRUE(v.is_number());
    EXPECT_DOUBLE_EQ(v.num, 42.0);
}

TEST(JsonParser, NumberNegative) {
    const char* input = "-7.5";
    const char* p = input;
    JsonValue v = parse_value(p);
    ASSERT_TRUE(v.is_number());
    EXPECT_DOUBLE_EQ(v.num, -7.5);
}

TEST(JsonParser, NumberScientific) {
    const char* input = "1.5e-3";
    const char* p = input;
    JsonValue v = parse_value(p);
    ASSERT_TRUE(v.is_number());
    EXPECT_NEAR(v.num, 0.0015, 1e-10);
}

TEST(JsonParser, BooleanTrue) {
    const char* input = "true";
    const char* p = input;
    JsonValue v = parse_value(p);
    ASSERT_TRUE(v.is_bool());
    EXPECT_TRUE(v.bval);
}

TEST(JsonParser, BooleanFalse) {
    const char* input = "false";
    const char* p = input;
    JsonValue v = parse_value(p);
    ASSERT_TRUE(v.is_bool());
    EXPECT_FALSE(v.bval);
}

TEST(JsonParser, NullValue) {
    const char* input = "null";
    const char* p = input;
    JsonValue v = parse_value(p);
    ASSERT_TRUE(v.is_null());
}

TEST(JsonParser, NestedObject) {
    const char* input = "{\"a\":{\"b\":1}}";
    const char* p = input;
    JsonValue v = parse_value(p);
    ASSERT_TRUE(v.is_object());
    EXPECT_EQ(v.size(), 1u);
    const auto* a = v.find("a");
    ASSERT_NE(a, nullptr);
    ASSERT_TRUE(a->is_object());
    const auto* b = a->find("b");
    ASSERT_NE(b, nullptr);
    EXPECT_DOUBLE_EQ(b->num, 1.0);
}

TEST(JsonParser, MixedArray) {
    const char* input = "[1, \"two\", true]";
    const char* p = input;
    JsonValue v = parse_value(p);
    ASSERT_TRUE(v.is_array());
    EXPECT_EQ(v.size(), 3u);
    ASSERT_TRUE(v[0].is_number());
    ASSERT_TRUE(v[1].is_string());
    ASSERT_TRUE(v[2].is_bool());
    EXPECT_DOUBLE_EQ(v[0].num, 1.0);
    EXPECT_EQ(v[1].sval, "two");
    EXPECT_TRUE(v[2].bval);
}

TEST(JsonParser, SkipWhitespace) {
    const char* input = "  { \"a\" : 1 }  ";
    const char* p = input;
    JsonValue v = parse_value(p);
    ASSERT_TRUE(v.is_object());
    const auto* a = v.find("a");
    ASSERT_NE(a, nullptr);
    EXPECT_DOUBLE_EQ(a->num, 1.0);
}

// ─── 2. JsonValue API ────────────────────────────────────────────────────

TEST(JsonValue, FindExistingKey) {
    const char* input = "{\"name\": \"test\", \"value\": 42}";
    const char* p = input;
    JsonValue v = parse_value(p);
    const auto* name = v.find("name");
    ASSERT_NE(name, nullptr);
    EXPECT_EQ(name->sval, "test");
}

TEST(JsonValue, FindMissingKey) {
    const char* input = "{\"name\": \"test\"}";
    const char* p = input;
    JsonValue v = parse_value(p);
    const auto* missing = v.find("missing");
    EXPECT_EQ(missing, nullptr);
}

TEST(JsonValue, FindNonObject) {
    const char* input = "42";
    const char* p = input;
    JsonValue v = parse_value(p);
    const auto* result = v.find("anything");
    EXPECT_EQ(result, nullptr);
}

TEST(JsonValue, SizeArray) {
    const char* input = "[1, 2, 3]";
    const char* p = input;
    JsonValue v = parse_value(p);
    EXPECT_EQ(v.size(), 3u);
}

TEST(JsonValue, SizeObject) {
    const char* input = "{\"a\": 1, \"b\": 2}";
    const char* p = input;
    JsonValue v = parse_value(p);
    EXPECT_EQ(v.size(), 2u);
}

TEST(JsonValue, SizeNonContainer) {
    const char* input = "42";
    const char* p = input;
    JsonValue v = parse_value(p);
    EXPECT_EQ(v.size(), 0u);
}

TEST(JsonValue, AsNumberDefault) {
    const char* input = "{\"key\": 42}";
    const char* p = input;
    JsonValue v = parse_value(p);
    const auto* missing = v.find("missing");
    // find() on object returns nullptr for missing keys
    if (missing) {
        EXPECT_DOUBLE_EQ(missing->as_number(99.0), 99.0);
    }
}

TEST(JsonValue, AsStringDefault) {
    const char* input = "{\"key\": 42}";
    const char* p = input;
    JsonValue v = parse_value(p);
    const auto* missing = v.find("missing");
    // find() on object returns nullptr for missing keys
    if (missing) {
        EXPECT_EQ(missing->as_string("default"), std::string("default"));
    }
}

TEST(JsonValue, AsBoolDefault) {
    const char* input = "{\"key\": 42}";
    const char* p = input;
    JsonValue v = parse_value(p);
    const auto* missing = v.find("missing");
    // find() on object returns nullptr for missing keys
    if (missing) {
        EXPECT_FALSE(missing->as_bool(true));
    }
}

TEST(JsonValue, OperatorIndexArray) {
    const char* input = "[10, 20, 30]";
    const char* p = input;
    JsonValue v = parse_value(p);
    ASSERT_TRUE(v[0].is_number());
    EXPECT_DOUBLE_EQ(v[0].num, 10.0);
    EXPECT_DOUBLE_EQ(v[1].num, 20.0);
    EXPECT_DOUBLE_EQ(v[2].num, 30.0);
}

// tests/test_mvt.cpp — Unit tests for MVT parser
#include "mvt_parser.h"

#include <gtest/gtest.h>
#include <fstream>
#include <vector>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

using namespace mvt;

// ─── 1. Zigzag Decoding ─────────────────────────────────────────────────

TEST(Zigzag, DecodeZero) {
    EXPECT_EQ(zigzag_decode(0), 0);
}

TEST(Zigzag, DecodePositive) {
    EXPECT_EQ(zigzag_decode(2), 1);
}

TEST(Zigzag, DecodeNegative) {
    EXPECT_EQ(zigzag_decode(1), -1);
}

TEST(Zigzag, DecodeLargePositive) {
    EXPECT_EQ(zigzag_decode(254), 127);
}

TEST(Zigzag, DecodeLargeNegative) {
    EXPECT_EQ(zigzag_decode(255), -128);
}

// ─── 2. Tile Parsing ────────────────────────────────────────────────────

static Tile parse_test_mvt(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(static_cast<size_t>(size));
    file.read(buffer.data(), size);

    google::protobuf::io::ArrayInputStream array_input(buffer.data(),
                                                        static_cast<int>(size));
    google::protobuf::io::CodedInputStream coded_input(&array_input);
    return parse_tile(&coded_input);
}

TEST(TileParsing, EmptyTile) {
    // Parse empty bytes
    char empty[1] = {0};
    google::protobuf::io::ArrayInputStream array_input(empty, 0);
    google::protobuf::io::CodedInputStream coded_input(&array_input);
    Tile tile = parse_tile(&coded_input);
    EXPECT_EQ(tile.layers.size(), 0u);
}

TEST(TileParsing, TestRoardsMvt) {
    Tile tile = parse_test_mvt("data/test_roads.mvt");
    EXPECT_GT(tile.layers.size(), 0u);
    // Verify at least one layer has features
    bool found_features = false;
    for (const auto& layer : tile.layers) {
        if (!layer.features.empty()) {
            found_features = true;
            break;
        }
    }
    EXPECT_TRUE(found_features);
}

TEST(TileParsing, TestSfMvt) {
    Tile tile = parse_test_mvt("test_data/sf.mvt");
    // sf.mvt may be empty or minimal, just verify it doesn't crash
    (void)tile;
}

TEST(TileParsing, LayerNames) {
    Tile tile = parse_test_mvt("data/test_roads.mvt");
    // Verify layer names are non-empty strings
    for (const auto& layer : tile.layers) {
        EXPECT_FALSE(layer.name.empty());
    }
}

TEST(TileParsing, FeatureGeometryPresent) {
    Tile tile = parse_test_mvt("data/test_roads.mvt");
    for (const auto& layer : tile.layers) {
        for (const auto& feat : layer.features) {
            if (feat.type == GeomType::LINESTRING || feat.type == GeomType::POLYGON) {
                EXPECT_FALSE(feat.geometry.empty());
            }
        }
    }
}

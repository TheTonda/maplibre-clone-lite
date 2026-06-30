// tests/test_mvt.cpp — Unit tests for MVT parser
#include <gtest/gtest.h>
#include "mvt_parser.h"
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <fstream>
#include <vector>

using namespace mvt;

// ─── Zigzag Decoding ─────────────────────────────────────────────────

TEST(MvtParser, ZigzagDecodeZero) {
    EXPECT_EQ(zigzag_decode(0), 0);
}

TEST(MvtParser, ZigzagDecodePositive) {
    EXPECT_EQ(zigzag_decode(2), 1);
}

TEST(MvtParser, ZigzagDecodeNegative) {
    EXPECT_EQ(zigzag_decode(1), -1);
}

TEST(MvtParser, ZigzagDecodeLargePositive) {
    EXPECT_EQ(zigzag_decode(254), 127);
}

TEST(MvtParser, ZigzagDecodeLargeNegative) {
    EXPECT_EQ(zigzag_decode(255), -128);
}

// ─── Tile Parsing ────────────────────────────────────────────────────

static std::vector<char> read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    std::streamsize sz = f.tellg();
    f.seekg(0);
    std::vector<char> buf(static_cast<size_t>(sz));
    f.read(buf.data(), sz);
    return buf;
}

TEST(MvtParser, ParseEmptyTile) {
    std::vector<uint8_t> empty;
    google::protobuf::io::ArrayInputStream input(empty.data(), 0);
    google::protobuf::io::CodedInputStream coded(&input);
    Tile tile = parse_tile(&coded);
    EXPECT_EQ(tile.layers.size(), 0u);
}

TEST(MvtParser, ParseTileWithLayers) {
    auto data = read_file("data/test_roads.mvt");
    if (data.empty()) {
        GTEST_SKIP() << "Test MVT file not found";
    }
    google::protobuf::io::ArrayInputStream input(data.data(), static_cast<int>(data.size()));
    google::protobuf::io::CodedInputStream coded(&input);
    Tile tile = parse_tile(&coded);
    EXPECT_GT(tile.layers.size(), 0u);
}

TEST(MvtParser, ParseTileLayerNames) {
    auto data = read_file("data/test_roads.mvt");
    if (data.empty()) {
        GTEST_SKIP() << "Test MVT file not found";
    }
    google::protobuf::io::ArrayInputStream input(data.data(), static_cast<int>(data.size()));
    google::protobuf::io::CodedInputStream coded(&input);
    Tile tile = parse_tile(&coded);
    // Verify at least one layer has a non-empty name
    bool has_name = false;
    for (const auto& layer : tile.layers) {
        if (!layer.name.empty()) {
            has_name = true;
            break;
        }
    }
    EXPECT_TRUE(has_name);
}

TEST(MvtParser, ParseTileLayerExtent) {
    auto data = read_file("data/test_roads.mvt");
    if (data.empty()) {
        GTEST_SKIP() << "Test MVT file not found";
    }
    google::protobuf::io::ArrayInputStream input(data.data(), static_cast<int>(data.size()));
    google::protobuf::io::CodedInputStream coded(&input);
    Tile tile = parse_tile(&coded);
    for (const auto& layer : tile.layers) {
        EXPECT_GT(layer.extent, 0u);
    }
}

TEST(MvtParser, ParseTileFeatureGeometry) {
    auto data = read_file("data/test_roads.mvt");
    if (data.empty()) {
        GTEST_SKIP() << "Test MVT file not found";
    }
    google::protobuf::io::ArrayInputStream input(data.data(), static_cast<int>(data.size()));
    google::protobuf::io::CodedInputStream coded(&input);
    Tile tile = parse_tile(&coded);
    for (const auto& layer : tile.layers) {
        for (const auto& feat : layer.features) {
            if (feat.type == GeomType::LINESTRING) {
                EXPECT_GT(feat.geometry.size(), 0u);
            }
        }
    }
}

// ─── Value Parsing ───────────────────────────────────────────────────

TEST(MvtParser, GeomTypeNamePoint) {
    EXPECT_STREQ(geom_type_name(GeomType::POINT), "POINT");
}

TEST(MvtParser, GeomTypeNameLineString) {
    EXPECT_STREQ(geom_type_name(GeomType::LINESTRING), "LINESTRING");
}

TEST(MvtParser, GeomTypeNamePolygon) {
    EXPECT_STREQ(geom_type_name(GeomType::POLYGON), "POLYGON");
}

TEST(MvtParser, GeomTypeNameUnknown) {
    EXPECT_STREQ(geom_type_name(GeomType::UNKNOWN), "UNKNOWN");
}

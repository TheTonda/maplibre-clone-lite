#pragma once
// mvt_parser.h — Header-only Mapbox Vector Tile parser for C++23
// Parses .mvt files using protobuf-lite (CodedInputStream).
// Milestone 2: reads a tile and prints feature counts per layer.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <google/protobuf/io/coded_stream.h>

#ifdef MAP_RENDERER_DEBUG
#define DEBUG_LOG(...) std::printf("[DEBUG] " __VA_ARGS__); std::printf("\n")
#else
#define DEBUG_LOG(...) ((void)0)
#endif

namespace mvt {

// ─── Data structures ───────────────────────────────────────────────

enum class GeomType : int {
    UNKNOWN    = 0,
    POINT      = 1,
    LINESTRING = 2,
    POLYGON    = 3,
};

struct Value {
    enum Type { STRING_VAL, FLOAT_VAL, DOUBLE_VAL, INT_VAL,
                UINT_VAL, SINT_VAL, BOOL_VAL, NONE };
    Type type = NONE;
    std::string string_value;
    double   numeric_value = 0.0;
    int64_t  int_value     = 0;
    bool     bool_value    = false;
};

struct Feature {
    uint64_t id = 0;
    GeomType type = GeomType::UNKNOWN;
    // tags are (key_index, value_index) pairs indexing into layer.keys / layer.values
    std::vector<std::pair<uint32_t, uint32_t>> tags;
    // raw geometry command stream (MoveTo, LineTo, ClosePath with zigzag params)
    std::vector<uint32_t> geometry;
};

struct Layer {
    std::string name;
    uint32_t version = 1;
    uint32_t extent  = 4096;
    std::vector<std::string> keys;    // key string table
    std::vector<Value>      values;   // value table
    std::vector<Feature>    features;
};

struct Tile {
    std::vector<Layer> layers;
};

// ─── Zigzag decoding ───────────────────────────────────────────────

/// MVT standard zigzag: ((n >> 1) ^ (-(n & 1)))
inline int32_t zigzag_decode(uint32_t n) {
    return static_cast<int32_t>((n >> 1) ^ (-(n & 1)));
}

// ─── Protobuf wire format skipping ─────────────────────────────────

namespace detail {

/// Skip over one protobuf field given its tag.
inline void skip_field(google::protobuf::io::CodedInputStream* stream,
                       uint32_t tag) {
    int wire_type = tag & 7;
    switch (wire_type) {
        case 0: { // varint
            uint32_t dummy;
            (void)stream->ReadVarint32(&dummy);
            break;
        }
        case 1: { // fixed64
            uint64_t dummy;
            (void)stream->ReadLittleEndian64(&dummy);
            break;
        }
        case 2: { // length-delimited
            uint32_t len;
            if (stream->ReadVarint32(&len))
                (void)stream->Skip(static_cast<int>(len));
            break;
        }
        case 5: { // fixed32
            uint32_t dummy;
            (void)stream->ReadLittleEndian32(&dummy);
            break;
        }
        default:
            break;
    }
}

// Parse a single Value submessage from a length-delimited blob.
inline Value parse_value(google::protobuf::io::CodedInputStream* stream,
                         uint32_t len) {
    Value val;
    auto limit = stream->PushLimit(static_cast<int>(len));
    uint32_t tag;
    while ((tag = stream->ReadTag()) != 0) {
        int field = tag >> 3;
        switch (field) {
            case 1: { // string_value
                uint32_t slen;
                if (stream->ReadVarint32(&slen)) {
                    std::string s;
                    (void)stream->ReadString(&s, static_cast<int>(slen));
                    val.string_value = std::move(s);
                    val.type = Value::STRING_VAL;
                }
                break;
            }
            case 2: { // float_value (fixed32)
                uint32_t raw;
                if (stream->ReadLittleEndian32(&raw)) {
                    float f;
                    std::memcpy(&f, &raw, sizeof(f));
                    val.numeric_value = static_cast<double>(f);
                    val.type = Value::FLOAT_VAL;
                }
                break;
            }
            case 3: { // double_value (fixed64)
                uint64_t raw;
                if (stream->ReadLittleEndian64(&raw)) {
                    double d;
                    std::memcpy(&d, &raw, sizeof(d));
                    val.numeric_value = d;
                    val.type = Value::DOUBLE_VAL;
                }
                break;
            }
            case 4: { // int_value (varint int64)
                uint64_t v;
                if (stream->ReadVarint64(&v)) {
                    val.int_value = static_cast<int64_t>(v);
                    val.numeric_value = static_cast<double>(val.int_value);
                    val.type = Value::INT_VAL;
                }
                break;
            }
            case 5: { // uint_value (varint)
                uint64_t v;
                if (stream->ReadVarint64(&v)) {
                    val.int_value = static_cast<int64_t>(v);
                    val.numeric_value = static_cast<double>(v);
                    val.type = Value::UINT_VAL;
                }
                break;
            }
            case 6: { // sint_value (varint, zigzag-encoded on wire)
                uint64_t v;
                if (stream->ReadVarint64(&v)) {
                    // sint64 is zigzag-encoded on the wire
                    val.int_value = static_cast<int64_t>((v >> 1) ^
                                                         (-(v & 1)));
                    val.numeric_value = static_cast<double>(val.int_value);
                    val.type = Value::SINT_VAL;
                }
                break;
            }
            case 7: { // bool_value (varint)
                uint32_t v;
                if (stream->ReadVarint32(&v)) {
                    val.bool_value = (v != 0);
                    val.type = Value::BOOL_VAL;
                }
                break;
            }
            default:
                skip_field(stream, tag);
                break;
        }
    }
    stream->PopLimit(limit);
    return val;
}

} // namespace detail

// ─── Main parse entry point ────────────────────────────────────────

/// Parse a full MVT tile from a CodedInputStream.
inline Tile parse_tile(google::protobuf::io::CodedInputStream* stream) {
    Tile tile;
    uint32_t tag;
    while ((tag = stream->ReadTag()) != 0) {
        int field = tag >> 3;
        int wire  = tag & 7;
        if (field == 3 && wire == 2) {
            // ── Layer (repeated, length-delimited) ──
            auto layer_limit = stream->ReadLengthAndPushLimit();
            Layer layer;
            uint32_t inner_tag;
            while ((inner_tag = stream->ReadTag()) != 0) {
                int fnum = inner_tag >> 3;
                int w    = inner_tag & 7;
                switch (fnum) {
                    // ── name (string) ──
                    case 1: {
                        if (w == 2) {
                            uint32_t slen;
                            if (stream->ReadVarint32(&slen)) {
                                std::string s;
                                (void)stream->ReadString(&s, static_cast<int>(slen));
                                layer.name = std::move(s);
                            }
                        }
                        break;
                    }
                    // ── features ──
                    case 2: {
                        if (w == 2) {
                            auto feat_limit = stream->ReadLengthAndPushLimit();
                            Feature feat;
                            uint32_t ftag;
                            while ((ftag = stream->ReadTag()) != 0) {
                                int fn = ftag >> 3;
                                int fw = ftag & 7;
                                switch (fn) {
                                    case 1: { // id (uint64, varint)
                                        uint64_t id;
                                        if (fw == 0 && stream->ReadVarint64(&id))
                                            feat.id = id;
                                        break;
                                    }
                                    case 2: { // tags [packed=true]
                                        if (fw == 2) {
                                            auto pack_lim = stream->ReadLengthAndPushLimit();
                                            std::vector<uint32_t> raw_tags;
                                            while (stream->BytesUntilLimit() > 0) {
                                                uint32_t v;
                                                if (stream->ReadVarint32(&v))
                                                    raw_tags.push_back(v);
                                            }
                                            stream->PopLimit(pack_lim);
                                            // pair up (key_index, value_index)
                                            for (size_t i = 0; i + 1 < raw_tags.size(); i += 2) {
                                                feat.tags.emplace_back(raw_tags[i],
                                                                       raw_tags[i + 1]);
                                            }
                                        }
                                        break;
                                    }
                                    case 3: { // type (GeomType enum)
                                        uint32_t gt;
                                        if (fw == 0 && stream->ReadVarint32(&gt))
                                            feat.type = static_cast<GeomType>(gt);
                                        break;
                                    }
                                    case 4: { // geometry [packed=true]
                                        if (fw == 2) {
                                            auto pack_lim = stream->ReadLengthAndPushLimit();
                                            while (stream->BytesUntilLimit() > 0) {
                                                uint32_t v;
                                                if (stream->ReadVarint32(&v))
                                                    feat.geometry.push_back(v);
                                            }
                                            stream->PopLimit(pack_lim);
                                        }
                                        break;
                                    }
                                    default:
                                        detail::skip_field(stream, ftag);
                                        break;
                                }
                            }
                            layer.features.push_back(std::move(feat));
                            stream->PopLimit(feat_limit);
                        }
                        break;
                    }
                    // ── keys (string table) ──
                    case 3: {
                        if (w == 2) {
                            // Wire type 2: length-delimited → a UTF-8 string key
                            uint32_t slen;
                            if (stream->ReadVarint32(&slen)) {
                                std::string s;
                                (void)stream->ReadString(&s, static_cast<int>(slen));
                                layer.keys.push_back(std::move(s));
                            }
                        } else if (w == 0) {
                            // Wire type 0: varint (numeric key, rare/custom)
                            uint32_t v;
                            (void)stream->ReadVarint32(&v);
                            layer.keys.push_back(std::to_string(v));
                        } else {
                            detail::skip_field(stream, inner_tag);
                        }
                        break;
                    }
                    // ── values (Value submessages) ──
                    case 4: {
                        if (w == 2) {
                            uint32_t val_len;
                            if (stream->ReadVarint32(&val_len)) {
                                layer.values.push_back(
                                    detail::parse_value(stream, val_len));
                            }
                        }
                        break;
                    }
                    // ── extent (uint32) ──
                    case 5: {
                        uint32_t ext;
                        if (w == 0 && stream->ReadVarint32(&ext))
                            layer.extent = ext;
                        break;
                    }
                    // ── version (uint32) ──
                    case 15: {
                        uint32_t ver;
                        if (w == 0 && stream->ReadVarint32(&ver))
                            layer.version = ver;
                        break;
                    }
                    default:
                        detail::skip_field(stream, inner_tag);
                        break;
                }
            }
            tile.layers.push_back(std::move(layer));
            stream->PopLimit(layer_limit);
        } else {
            detail::skip_field(stream, tag);
        }
    }
    return tile;
}

// ─── Utility: print summary ────────────────────────────────────────

inline const char* geom_type_name(GeomType t) {
    switch (t) {
        case GeomType::POINT:      return "POINT";
        case GeomType::LINESTRING: return "LINESTRING";
        case GeomType::POLYGON:    return "POLYGON";
        default:                   return "UNKNOWN";
    }
}

/// Print feature counts per layer.
inline void print_summary(const Tile& tile) {
    DEBUG_LOG("=== MVT Tile Summary ===");
    std::printf("=== MVT Tile Summary ===\n");
    std::printf("Total layers: %zu\n\n", tile.layers.size());

    for (const auto& layer : tile.layers) {
        int counts[4] = {0, 0, 0, 0}; // UNKNOWN, POINT, LINESTRING, POLYGON

        for (const auto& feat : layer.features) {
            int idx = static_cast<int>(feat.type);
            if (idx >= 0 && idx < 4) counts[idx]++;
        }

        std::printf("Layer: \"%s\"\n", layer.name.c_str());
        std::printf("  Version: %u, Extent: %u\n", layer.version, layer.extent);
        std::printf("  Keys: %zu, Values: %zu\n", layer.keys.size(), layer.values.size());
        std::printf("  Features: %zu total\n", layer.features.size());
        std::printf("    POINT:      %d\n", counts[1]);
        std::printf("    LINESTRING: %d\n", counts[2]);
        std::printf("    POLYGON:    %d\n", counts[3]);
        if (counts[0] > 0)
            std::printf("    UNKNOWN:    %d\n", counts[0]);

        // Print a few sample zigzag-decoded coordinates from first feature
        if (!layer.features.empty() && !layer.features[0].geometry.empty()) {
            const auto& geom = layer.features[0].geometry;
            std::printf("  Sample geometry (first feature, first command): ");
            uint32_t cmd = geom[0];
            int cmd_id = cmd & 0x7;
            int count = cmd >> 3;
            std::printf("cmd=%u (id=%d count=%d)", cmd, cmd_id, count);
            if ((cmd_id == 1 || cmd_id == 2) && geom.size() > 1) {
                std::printf(" coords:");
                size_t limit = std::min(geom.size() - 1,
                                        static_cast<size_t>(count * 2));
                for (size_t i = 1; i <= limit && i < geom.size(); ++i) {
                    int32_t coord = zigzag_decode(geom[i]);
                    std::printf(" %d", coord);
                }
            }
            std::printf("\n");
        }

        std::printf("\n");
    }
}

} // namespace mvt

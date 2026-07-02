#pragma once

#include <cstdint>
#include <functional>

namespace map_renderer {

struct TileId {
    uint32_t z = 0;  // zoom level
    uint32_t x = 0;  // tile column
    uint32_t y = 0;  // tile row

    bool operator==(const TileId& other) const {
        return z == other.z && x == other.x && y == other.y;
    }

    bool operator!=(const TileId& other) const {
        return !(*this == other);
    }

    // For use as key in unordered_map.
    // Uses hash-combine (boost::hash_combine style) so it works correctly
    // for all valid zoom levels (z up to 20+, x/y up to 2^z).
    struct Hash {
        size_t operator()(const TileId& t) const {
            size_t h = std::hash<uint32_t>{}(t.z);
            h ^= std::hash<uint32_t>{}(t.x)
                 + 0x9e3779b9u + (h << 6) + (h >> 2);
            h ^= std::hash<uint32_t>{}(t.y)
                 + 0x9e3779b9u + (h << 6) + (h >> 2);
            return h;
        }
    };
};

} // namespace map_renderer

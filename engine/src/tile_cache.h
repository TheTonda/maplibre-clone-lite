#pragma once

#include "mbtiles_reader.h"
#include "webp_decoder.h"

#include <list>
#include <optional>
#include <unordered_map>
#include <vector>

namespace maprender {

class TileCache {
public:
    struct Bitmap { int w = 0, h = 0; std::vector<unsigned char> px; };

    explicit TileCache(MBTilesReader& reader, size_t max_bitmap_bytes);

    // Returns a pointer to the decoded RGBA8 bitmap (caller does NOT own it),
    // or nullptr if the tile is missing / undecodable. Lookups promote LRU.
    const Bitmap* get(int z, int x, int slippy_y);

    size_t bytes_used() const { return bytes_used_; }
    size_t max_bytes()   const { return max_bytes_; }

private:
    struct Key { int z, x, y; };
    struct KeyHash { size_t operator()(const Key& k) const noexcept {
        return (size_t)std::hash<long long>{}(((long long)k.z << 42) ^ ((long long)k.x << 21) ^ k.y);
    } };
    struct KeyEq { bool operator()(const Key& a, const Key& b) const noexcept {
        return a.z == b.z && a.x == b.x && a.y == b.y;
    } };

    struct Entry { Key key; Bitmap bmp; };
    using Iter = std::list<Entry>::iterator;

    MBTilesReader& reader_;
    size_t max_bytes_;
    size_t bytes_used_ = 0;
    std::list<Entry> lru_;  // front = most recently used
    std::unordered_map<Key, Iter, KeyHash, KeyEq> map_;

    void touch(Iter it);
    void evict_until(size_t target);
};

}  // namespace maprender
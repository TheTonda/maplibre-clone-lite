#include "tile_cache.h"

#include <algorithm>

namespace maprender {

TileCache::TileCache(MBTilesReader& reader, size_t max_bitmap_bytes)
    : reader_(reader), max_bytes_(max_bitmap_bytes) {}

void TileCache::touch(Iter it) {
    lru_.splice(lru_.begin(), lru_, it);
}

void TileCache::evict_until(size_t target) {
    while (bytes_used_ > target && !lru_.empty()) {
        auto last = std::prev(lru_.end());
        bytes_used_ -= last->bmp.px.size();
        map_.erase(last->key);
        lru_.pop_back();
    }
}

const TileCache::Bitmap* TileCache::get(int z, int x, int slippy_y) {
    const Key key{z, x, slippy_y};
    if (auto it = map_.find(key); it != map_.end()) {
        touch(it->second);
        return &it->second->bmp;
    }
    std::vector<unsigned char> blob;
    if (!reader_.read_tile(z, x, slippy_y, blob)) return nullptr;

    Entry e;
    e.key = key;
    int w = 0, h = 0;
    if (!webp_decode_rgba(blob.data(), blob.size(), w, h, e.bmp.px)) return nullptr;
    e.bmp.w = w;
    e.bmp.h = h;

    const size_t sz = e.bmp.px.size();
    if (sz > max_bytes_) return nullptr;  // single tile larger than cache
    evict_until(max_bytes_ - sz);

    lru_.push_front(std::move(e));
    auto inserted = lru_.begin();
    bytes_used_ += sz;
    map_[key] = inserted;
    return &inserted->bmp;
}

}  // namespace maprender
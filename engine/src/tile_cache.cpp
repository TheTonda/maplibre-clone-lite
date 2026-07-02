#include "map_renderer/tile_cache.h"

namespace map_renderer {

TileCache::TileCache(size_t max_tiles)
    : max_tiles_(max_tiles)
{
}

std::shared_ptr<TileData> TileCache::get(const TileId& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(id);
    if (it == cache_.end()) {
        return nullptr;
    }
    // Move to front of LRU list (most recently used)
    lru_order_.remove(id);
    lru_order_.push_front(id);
    return it->second;
}

void TileCache::put(const TileId& id, std::shared_ptr<TileData> tile) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Evict if at capacity and this is a new tile
    if (cache_.find(id) == cache_.end() && cache_.size() >= max_tiles_) {
        evict_lru();
    }
    cache_[id] = tile;
    lru_order_.remove(id);
    lru_order_.push_front(id);
    recent_inserts_.push_back(id);
}

std::vector<TileId> TileCache::drain_recent_inserts() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TileId> result;
    result.swap(recent_inserts_);
    return result;
}

void TileCache::invalidate(const TileId& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(id);
    if (it != cache_.end()) {
        if (eviction_cb_) {
            eviction_cb_(id, *it->second);
        }
        cache_.erase(it);
        lru_order_.remove(id);
    }
}

void TileCache::set_eviction_callback(std::function<void(const TileId&, TileData&)> cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    eviction_cb_ = std::move(cb);
}

size_t TileCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.size();
}

size_t TileCache::capacity() const {
    return max_tiles_;
}

void TileCache::evict_lru() {
    if (lru_order_.empty()) return;
    TileId evict_id = lru_order_.back();
    lru_order_.pop_back();
    auto it = cache_.find(evict_id);
    if (it != cache_.end()) {
        if (eviction_cb_) {
            eviction_cb_(evict_id, *it->second);
        }
        cache_.erase(it);
    }
}

} // namespace map_renderer

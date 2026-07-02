#pragma once

#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "osm_types.h"
#include "tile_id.h"

namespace map_renderer {

class TileCache {
public:
    explicit TileCache(size_t max_tiles = 64);

    // Called by render thread: returns tile if loaded, nullptr if not.
    // Marks tile as recently used.
    std::shared_ptr<TileData> get(const TileId& id);

    // Called by loader thread: inserts a newly loaded tile.
    // Records the id in an insert-log for drain_recent_inserts().
    // Evicts LRU tile if over capacity (frees GPU buffers via callback).
    void put(const TileId& id, std::shared_ptr<TileData> tile);

    // Called by render thread (Engine): returns and clears the list of
    // tile IDs inserted since the last call. The Engine uses this to know
    // which tiles need GPU upload without giving the loader thread a
    // Renderer pointer.
    std::vector<TileId> drain_recent_inserts();

    // Called by render thread: mark tile for removal
    void invalidate(const TileId& id);

    // Set callback for GPU resource cleanup on eviction
    void set_eviction_callback(std::function<void(const TileId&, TileData&)> cb);

    // Stats
    size_t size() const;
    size_t capacity() const;

private:
    size_t max_tiles_;
    mutable std::mutex mutex_;
    std::unordered_map<TileId, std::shared_ptr<TileData>, TileId::Hash> cache_;
    std::list<TileId> lru_order_;  // front = most recent
    std::vector<TileId> recent_inserts_;
    std::function<void(const TileId&, TileData&)> eviction_cb_;

    void evict_lru();
};

} // namespace map_renderer

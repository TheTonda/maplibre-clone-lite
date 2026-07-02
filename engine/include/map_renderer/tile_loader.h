#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "tile_cache.h"
#include "tile_id.h"

namespace map_renderer {

// Background thread that reads tile files from storage, decompresses zstd,
// and hands deserialized TileData to the TileCache. Delegates protobuf
// parsing to OSMLoader.
class TileLoader {
public:
    TileLoader(const std::string& tile_dir, TileCache& cache,
               double ref_lat, double ref_lon);
    ~TileLoader();

    // Start background thread
    void start();

    // Stop background thread (waits for current load to finish)
    void stop();

    // Called by render thread: request a set of tiles to be loaded
    void request_tiles(const std::vector<TileId>& tiles);

    // Called by render thread: cancel loads for tiles no longer needed
    void cancel_tiles(const std::vector<TileId>& tiles);

    // File path: tile_dir/z/x/y.bin
    static std::string tile_path(const std::string& dir, const TileId& id);

private:
    std::string tile_dir_;
    TileCache& cache_;
    double ref_lat_;
    double ref_lon_;
    std::thread worker_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::unordered_set<TileId, TileId::Hash> pending_;
    std::atomic<bool> running_{false};

    void worker_loop();
    bool load_tile(const TileId& id);
};

} // namespace map_renderer

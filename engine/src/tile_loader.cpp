#include "map_renderer/tile_loader.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

#include <zstd.h>

#include "map_renderer/debug_log.h"
#include "map_renderer/osm_loader.h"

namespace map_renderer {

TileLoader::TileLoader(const std::string& tile_dir, TileCache& cache,
                       double ref_lat, double ref_lon)
    : tile_dir_(tile_dir)
    , cache_(cache)
    , ref_lat_(ref_lat)
    , ref_lon_(ref_lon)
{
}

TileLoader::~TileLoader() {
    stop();
}

void TileLoader::start() {
    if (running_.exchange(true)) return;  // already running
    worker_ = std::thread(&TileLoader::worker_loop, this);
}

void TileLoader::stop() {
    if (!running_.exchange(false)) return;  // not running
    queue_cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void TileLoader::request_tiles(const std::vector<TileId>& tiles) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    for (const auto& t : tiles) {
        // Only add if not already pending and not in cache
        pending_.insert(t);
    }
    queue_cv_.notify_one();
}

void TileLoader::cancel_tiles(const std::vector<TileId>& tiles) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    for (const auto& t : tiles) {
        pending_.erase(t);
    }
}

void TileLoader::worker_loop() {
    while (running_) {
        TileId tile_id{};
        bool has_work = false;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this]() {
                return !running_ || !pending_.empty();
            });

            if (!running_) break;

            if (!pending_.empty()) {
                auto it = pending_.begin();
                tile_id = *it;
                pending_.erase(it);
                has_work = true;
            }
        }

        if (has_work) {
            load_tile(tile_id);
        }
    }
}

bool TileLoader::load_tile(const TileId& id) {
    // Check if tile is already in cache
    if (cache_.get(id)) {
        return true;
    }

    // Load file
    std::string path = tile_path(tile_dir_, id);
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        // Missing file is normal for empty or not-yet-generated tiles
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> compressed(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(compressed.data()), size)) {
        DEBUG_LOG("Failed to read tile file: %s", path.c_str());
        return false;
    }
    file.close();

    // Decompress zstd
    unsigned long long const decomp_size = ZSTD_getFrameContentSize(
        compressed.data(), compressed.size());
    if (decomp_size == ZSTD_CONTENTSIZE_ERROR || decomp_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        DEBUG_LOG("Invalid zstd frame in tile: %s", path.c_str());
        return false;
    }

    std::vector<uint8_t> decompressed(static_cast<size_t>(decomp_size));
    size_t result = ZSTD_decompress(decompressed.data(), decompressed.size(),
                                     compressed.data(), compressed.size());
    if (ZSTD_isError(result)) {
        DEBUG_LOG("zstd decompression error in tile: %s", path.c_str());
        return false;
    }

    // Deserialize protobuf
    auto tile = std::make_shared<TileData>();
    tile->id = id;

    if (!OSMLoader::deserialize(decompressed, id, ref_lat_, ref_lon_, *tile)) {
        DEBUG_LOG("Protobuf deserialization error in tile: %s", path.c_str());
        return false;
    }

    // Insert into cache
    cache_.put(id, tile);
    return true;
}

std::string TileLoader::tile_path(const std::string& dir, const TileId& id) {
    std::ostringstream oss;
    oss << dir << "/" << id.z << "/" << id.x << "/" << id.y << ".bin";
    return oss.str();
}

} // namespace map_renderer

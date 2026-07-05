// Engine orchestrator — wires camera, cache, loader, renderer
// Implements the update loop per LLD §8.

#include "map_renderer/engine.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <vector>

#include <zstd.h>

#include "map_renderer/debug_log.h"
#include "map_renderer/osm_loader.h"
#include "map_renderer/renderer.h"
#include "map_renderer/tile_cache.h"
#include "map_renderer/tile_loader.h"
#include "osm_data.pb.h"

namespace map_renderer {

namespace {
constexpr double R = 6371000.0;
constexpr double DEG_TO_RAD = M_PI / 180.0;

// WGS84 → world ENU (must match preprocessor and camera exactly)
void lat_lon_to_xy(double lat, double lon, double ref_lat, double ref_lon,
                   float& out_x, float& out_z) {
    out_x = static_cast<float>(
        R * std::cos(ref_lat * DEG_TO_RAD) * ((lon - ref_lon) * DEG_TO_RAD));
    out_z = static_cast<float>(R * (lat - ref_lat) * DEG_TO_RAD);
}
} // namespace

Engine::Engine() = default;
Engine::~Engine() { shutdown(); }

bool Engine::initialize(PlatformInterface& platform, const std::string& dataset_name) {
    platform_ = &platform;

    // Load metadata
    load_metadata(dataset_name);

    // Create subsystems
    cache_ = std::make_unique<TileCache>(64);
    renderer_ = std::make_unique<Renderer>();
    loader_ = std::make_unique<TileLoader>(platform.get_tile_data_path(), *cache_,
                                           ref_lat_, ref_lon_);

    if (!renderer_->initialize(platform)) {
        return false;
    }

    // Set eviction callback: free GPU resources when tile is evicted
    cache_->set_eviction_callback(
        [this](const TileId& id, TileData& tile) {
            renderer_->on_tile_evicted(id, tile);
        });

    // Set camera reference point and bounds, then frame the dataset
    camera_.set_reference_point(ref_lat_, ref_lon_);
    camera_.set_dataset_bounds(min_x_, max_x_, min_z_, max_z_);
    camera_.frame_dataset();

    // Zoom in to a practical starting level where roads are visible.
    // frame_dataset() shows the whole dataset (~50 km for New Delhi) which
    // makes 6 m roads sub-pixel. Target ~2 km span: roads ≈ 3 px at 1024 px.
    while (camera_.get_visible_span() > 2000.0f) {
        camera_.zoom_by(0.8f);
    }

    // Start background tile loader
    loader_->start();

    return true;
}

void Engine::update(const std::vector<InputData>& input_events, float dt) {
    // 1. Process input events
    for (const auto& event : input_events) {
        if (event.type == InputEvent::KeyQuit) {
            quit_ = true;
        }
        camera_.apply_input(event, dt);
    }

    // 2. If camera moved, recompute visible tiles and request/cancel
    if (camera_.is_dirty()) {
        recompute_visible_tiles();

        // Request prefetch tiles (which includes all visible tiles)
        loader_->request_tiles(prefetch_tiles_);

        camera_.clear_dirty();
    }

    // 3. Upload newly loaded tiles to GPU
    drain_pending_uploads();

    // 4. Render
    if (!visible_tiles_.empty()) {
        renderer_->render(camera_, *cache_, visible_tiles_);
    }
}

void Engine::on_resize(int /*width*/, int /*height*/) {
    // Viewport dimensions changed — force camera to recompute matrices
    // and visible tile set next frame. The platform's get_viewport_width/height()
    // returns the new size; the camera reads it via the platform.
    camera_.mark_dirty();
}

bool Engine::should_quit() const {
    return quit_;
}

void Engine::shutdown() {
    if (loader_) loader_->stop();
    if (renderer_) renderer_->cleanup();
    if (cache_) cache_.reset();
    loader_.reset();
    renderer_.reset();
}

void Engine::load_metadata(const std::string& dataset_name) {
    std::string meta_path = dataset_name + "/metadata.bin";
    std::ifstream f(meta_path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) {
        // No metadata — use reasonable defaults for New Delhi area
        ref_lat_ = 28.589;
        ref_lon_ = 77.2375;
        min_x_ = -20000.0f;
        max_x_ = 20000.0f;
        min_z_ = -20000.0f;
        max_z_ = 20000.0f;
        DEBUG_LOG("%s", "No metadata.bin found, using defaults");
        return;
    }

    auto size = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> compressed(static_cast<size_t>(size));
    f.read(reinterpret_cast<char*>(compressed.data()), size);
    f.close();

    auto decomp_size = ZSTD_getFrameContentSize(compressed.data(), compressed.size());
    if (decomp_size == ZSTD_CONTENTSIZE_ERROR || decomp_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        DEBUG_LOG("%s", "Invalid zstd frame in metadata.bin");
        return;
    }

    std::vector<uint8_t> decompressed(static_cast<size_t>(decomp_size));
    if (ZSTD_isError(ZSTD_decompress(decompressed.data(), decompressed.size(),
                                     compressed.data(), compressed.size()))) {
        DEBUG_LOG("%s", "zstd decompression error in metadata.bin");
        return;
    }

    map_renderer_pb::DatasetMetadata meta;
    if (!meta.ParseFromArray(decompressed.data(), static_cast<int>(decompressed.size()))) {
        DEBUG_LOG("%s", "Failed to parse metadata protobuf");
        return;
    }

    ref_lat_ = meta.ref_lat();
    ref_lon_ = meta.ref_lon();

    // Convert lat/lon bounds to world ENU.
    // x depends on lon only (R * cos(ref_lat) * Δlon).
    // z depends on lat only (R * Δlat).
    float unused;
    lat_lon_to_xy(ref_lat_, meta.min_lon(), ref_lat_, ref_lon_, min_x_, unused);
    lat_lon_to_xy(ref_lat_, meta.max_lon(), ref_lat_, ref_lon_, max_x_, unused);
    lat_lon_to_xy(meta.min_lat(), ref_lon_, ref_lat_, ref_lon_, unused, min_z_);
    lat_lon_to_xy(meta.max_lat(), ref_lon_, ref_lat_, ref_lon_, unused, max_z_);

    // Ensure min < max
    if (min_x_ > max_x_) std::swap(min_x_, max_x_);
    if (min_z_ > max_z_) std::swap(min_z_, max_z_);
}

void Engine::recompute_visible_tiles() {
    uint32_t z = camera_.get_tile_zoom();
    visible_tiles_ = camera_.get_visible_tiles(z);

    // Compute prefetch ring: expand visible tile range by 1 in each direction
    if (visible_tiles_.empty()) {
        prefetch_tiles_.clear();
        return;
    }

    uint32_t min_x = visible_tiles_[0].x, max_x = min_x;
    uint32_t min_y = visible_tiles_[0].y, max_y = min_y;
    for (const auto& t : visible_tiles_) {
        if (t.x < min_x) min_x = t.x;
        if (t.x > max_x) max_x = t.x;
        if (t.y < min_y) min_y = t.y;
        if (t.y > max_y) max_y = t.y;
    }

    uint32_t max_tile = (1u << z) - 1;
    uint32_t start_x = (min_x > 0) ? min_x - 1 : 0;
    uint32_t end_x = std::min(max_x + 1, max_tile);
    uint32_t start_y = (min_y > 0) ? min_y - 1 : 0;
    uint32_t end_y = std::min(max_y + 1, max_tile);

    prefetch_tiles_.clear();
    for (uint32_t y = start_y; y <= end_y; ++y) {
        for (uint32_t x = start_x; x <= end_x; ++x) {
            prefetch_tiles_.push_back({z, x, y});
        }
    }
}

void Engine::drain_pending_uploads() {
    auto recent = cache_->drain_recent_inserts();
    for (const auto& id : recent) {
        auto tile = cache_->get(id);
        if (tile && !tile->uploaded) {
            renderer_->on_tile_loaded(id, *tile);
            tile->uploaded = true;

            // Free CPU feature vectors now that geometry is on GPU
            tile->buildings.clear();
            tile->buildings.shrink_to_fit();
            tile->roads.clear();
            tile->roads.shrink_to_fit();
            tile->polygons.clear();
            tile->polygons.shrink_to_fit();
        }
    }
}

} // namespace map_renderer
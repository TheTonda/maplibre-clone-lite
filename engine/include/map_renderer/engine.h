#pragma once

#include <memory>
#include <string>
#include <vector>

#include "camera.h"
#include "platform.h"
#include "tile_id.h"

namespace map_renderer {

class Renderer;
class TileCache;
class TileLoader;

class Engine {
public:
    Engine();
    ~Engine();

    // Initialize with platform
    bool initialize(PlatformInterface& platform, const std::string& dataset_name);

    // Called by app each frame — process input, update camera, load tiles, render
    void update(const std::vector<InputData>& input_events, float dt);

    // Called when viewport size changes
    void on_resize(int width, int height);

    // Check if engine wants to quit
    bool should_quit() const;

    // Cleanup
    void shutdown();

private:
    PlatformInterface* platform_ = nullptr;
    Camera camera_;
    std::unique_ptr<TileCache> cache_;
    std::unique_ptr<TileLoader> loader_;
    std::unique_ptr<Renderer> renderer_;

    bool quit_ = false;

    // Cached visible + prefetch tile lists — zero allocation in render loop
    std::vector<TileId> visible_tiles_;
    std::vector<TileId> prefetch_tiles_;

    // Dataset metadata
    double ref_lat_ = 0.0;
    double ref_lon_ = 0.0;
    float min_x_ = 0.0f, max_x_ = 0.0f, min_z_ = 0.0f, max_z_ = 0.0f;

    void load_metadata(const std::string& dataset_name);
    void recompute_visible_tiles();
    void drain_pending_uploads();
};

} // namespace map_renderer

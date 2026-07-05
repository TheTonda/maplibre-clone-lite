#pragma once

#include "context.h"
#include "tile_cache.h"
#include "viewport.h"

#include <memory>

namespace maprender {

class Renderer {
public:
    explicit Renderer(Context& ctx);

    void render();

    // Update ctx view fields and bump view_gen. Pan/zoom are pixel-based and
    // work in screen space (1 world px == 1 screen px at integer zoom).
    void pan(int dx_px, int dy_px);
    void zoom(int delta, double anchor_lon, double anchor_lat);

private:
    Context& ctx_;
    std::unique_ptr<TileCache> cache_;
    Viewport vp_{};
    long vp_gen_ = -1;

    void sync_viewport();
    void blit_tile(const TileCache::Bitmap& bmp, double origin_x, double origin_y);
};

}  // namespace maprender
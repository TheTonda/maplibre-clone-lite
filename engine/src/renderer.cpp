#include "renderer.h"

#include <algorithm>
#include <cstring>

namespace maprender {

Renderer::Renderer(Context& ctx) : ctx_(ctx) {
    cache_ = std::make_unique<TileCache>(*ctx_.reader, 64ull * 1024 * 1024);
}

void Renderer::sync_viewport() {
    if (vp_gen_ == ctx_.view_gen) return;
    vp_.set_view(ctx_.center_lon, ctx_.center_lat, ctx_.zoom,
                 ctx_.screen_w, ctx_.screen_h);
    vp_gen_ = ctx_.view_gen;
}

void Renderer::pan(int dx_px, int dy_px) {
    sync_viewport();
    vp_.pan(dx_px, dy_px);
    ctx_.center_lon = world_x_to_lon(vp_.center_x, vp_.zoom);
    ctx_.center_lat = world_y_to_lat(vp_.center_y, vp_.zoom);
    ctx_.view_gen++;
}

void Renderer::zoom(int delta, double anchor_lon, double anchor_lat) {
    sync_viewport();
    const int new_zoom = std::clamp(vp_.zoom + delta, ctx_.min_zoom, ctx_.max_zoom);
    if (new_zoom == vp_.zoom) return;
    vp_.zoom_by(delta, anchor_lon, anchor_lat);
    ctx_.zoom = vp_.zoom;
    ctx_.center_lon = world_x_to_lon(vp_.center_x, vp_.zoom);
    ctx_.center_lat = world_y_to_lat(vp_.center_y, vp_.zoom);
    ctx_.view_gen++;
}

void Renderer::blit_tile(const TileCache::Bitmap& bmp, double origin_x, double origin_y) {
    auto& frame = ctx_.frame;
    const int fw = frame.width;
    const int fh = frame.height;

    const int src_w = bmp.w;
    const int src_h = bmp.h;
    const int x0 = static_cast<int>(std::floor(origin_x));
    const int y0 = static_cast<int>(std::floor(origin_y));

    const int dst_x0 = std::max(0, x0);
    const int dst_y0 = std::max(0, y0);
    const int dst_x1 = std::min(fw, x0 + src_w);
    const int dst_y1 = std::min(fh, y0 + src_h);
    if (dst_x0 >= dst_x1 || dst_y0 >= dst_y1) return;

    const int src_off_x = dst_x0 - x0;
    const int src_off_y = dst_y0 - y0;
    const int copy_w = dst_x1 - dst_x0;
    const int copy_h = dst_y1 - dst_y0;

    for (int y = 0; y < copy_h; ++y) {
        const unsigned char* src = bmp.px.data() +
            (static_cast<size_t>(src_off_y + y) * src_w + src_off_x) * 4;
        unsigned char* dst = frame.pixels.data() +
            (static_cast<size_t>(dst_y0 + y) * fw + dst_x0) * 4;
        std::memcpy(dst, src, static_cast<size_t>(copy_w) * 4);
    }
}

void Renderer::render() {
    sync_viewport();

    auto& frame = ctx_.frame;
    if (frame.pixels.empty()) return;
    std::fill(frame.pixels.begin(), frame.pixels.end(),
              std::uint8_t{0xee});

    const int max_xy = vp_.max_tile_xy();
    const int tx_min = std::max(0, vp_.tile_x_min());
    const int tx_max = std::min(max_xy - 1, vp_.tile_x_max());
    const int ty_min = std::max(0, vp_.tile_y_min());
    const int ty_max = std::min(max_xy - 1, vp_.tile_y_max());

    const double origin_left = vp_.world_left();
    const double origin_top  = vp_.world_top();

    for (int ty = ty_min; ty <= ty_max; ++ty) {
        for (int tx = tx_min; tx <= tx_max; ++tx) {
            const TileCache::Bitmap* bmp = cache_->get(vp_.zoom, tx, ty);
            if (!bmp) continue;
            const double ox = tx * 256.0 - origin_left;
            const double oy = ty * 256.0 - origin_top;
            blit_tile(*bmp, ox, oy);
        }
    }
}

}  // namespace maprender
#include "maprender/c_api.h"

#include "context.h"

#include <new>
#include <stdexcept>
#include <string>

using maprender::Context;
using maprender::Frame;
using maprender::MBTilesReader;

namespace {
std::string g_last_error;

void set_error(Context* ctx, const std::string& msg) {
    if (ctx) ctx->last_error = msg;
    else g_last_error = msg;
}
}  // namespace

extern "C" {

MR_Context* mr_open(const char* mbtiles_path) {
    if (!mbtiles_path) {
        set_error(nullptr, "mr_open: null path");
        return nullptr;
    }
    try {
        auto ctx = std::make_unique<Context>();
        ctx->reader = std::make_unique<MBTilesReader>(mbtiles_path);
        if (!ctx->reader->open()) {
            set_error(ctx.get(), "mr_open: cannot open mbtiles");
            return nullptr;
        }
        return reinterpret_cast<MR_Context*>(ctx.release());
    } catch (const std::exception& e) {
        set_error(nullptr, std::string("mr_open: ") + e.what());
        return nullptr;
    }
}

void mr_close(MR_Context* ctx) {
    if (ctx) {
        delete reinterpret_cast<Context*>(ctx);
    }
}

int mr_min_zoom(MR_Context* ctx) {
    return ctx ? reinterpret_cast<Context*>(ctx)->reader->min_zoom() : 0;
}

int mr_max_zoom(MR_Context* ctx) {
    return ctx ? reinterpret_cast<Context*>(ctx)->reader->max_zoom() : 0;
}

void mr_bounds(MR_Context* ctx, double* w, double* s, double* e, double* n) {
    if (!ctx) return;
    auto* c = reinterpret_cast<Context*>(ctx);
    double bw, bs, be, bn;
    c->reader->bounds(bw, bs, be, bn);
    if (w) *w = bw;
    if (s) *s = bs;
    if (e) *e = be;
    if (n) *n = bn;
}

const char* mr_last_error(MR_Context* ctx) {
    if (ctx) {
        auto* c = reinterpret_cast<Context*>(ctx);
        return c->last_error.c_str();
    }
    return g_last_error.c_str();
}

void mr_set_view(MR_Context* ctx, double lon, double lat,
                 int zoom_int, int screen_w, int screen_h) {
    if (!ctx) return;
    auto* c = reinterpret_cast<Context*>(ctx);
    c->center_lon = lon;
    c->center_lat = lat;
    c->zoom = zoom_int;
    c->screen_w = screen_w;
    c->screen_h = screen_h;
    try {
        c->frame.width = screen_w;
        c->frame.height = screen_h;
        c->frame.pixels.assign(static_cast<size_t>(screen_w) * screen_h * 4, 0);
    } catch (...) {
        set_error(c, "mr_set_view: oom");
    }
}

void mr_pan(MR_Context* /*ctx*/, int /*dx_px*/, int /*dy_px*/) {
    // Phase 2
}

void mr_zoom(MR_Context* /*ctx*/, int /*delta*/,
             double /*anchor_lon*/, double /*anchor_lat*/) {
    // Phase 2
}

const MR_Frame* mr_render(MR_Context* /*ctx*/) {
    // Phase 2: composite visible tiles
    return nullptr;
}

const unsigned char* mr_frame_pixels(const MR_Frame* f) {
    if (!f) return nullptr;
    return reinterpret_cast<const Frame*>(f)->pixels.data();
}

int mr_frame_width(const MR_Frame* f) {
    return f ? reinterpret_cast<const Frame*>(f)->width : 0;
}

int mr_frame_height(const MR_Frame* f) {
    return f ? reinterpret_cast<const Frame*>(f)->height : 0;
}

}  // extern "C"
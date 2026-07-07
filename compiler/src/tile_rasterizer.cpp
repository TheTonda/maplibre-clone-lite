#include "tile_rasterizer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace mapbake {

TileRasterizer::TileRasterizer() : buf_(kSize * kSize * 4, 0) {}

void TileRasterizer::clear(uint32_t bg) {
    const uint8_t r = bg & 0xff;
    const uint8_t g = (bg >> 8) & 0xff;
    const uint8_t b = (bg >> 16) & 0xff;
    const uint8_t a = (bg >> 24) & 0xff;
    for (size_t i = 0; i < buf_.size(); i += 4) {
        buf_[i+0] = r; buf_[i+1] = g; buf_[i+2] = b; buf_[i+3] = a;
    }
}

void TileRasterizer::blend_pixel(int x, int y, uint32_t color, float cov) {
    if (x < 0 || x >= kSize || y < 0 || y >= kSize) return;
    if (cov <= 0.0f) return;
    cov = std::min(1.0f, cov);
    const size_t off = (static_cast<size_t>(y) * kSize + x) * 4;
    // Tiles are blitted by the engine with a plain memcpy (no alpha
    // compositing), so they must stay fully opaque.  Antialiasing is achieved
    // by blending the source colour into the existing opaque background in
    // proportion to coverage, keeping alpha at 255.  Writing partial alpha
    // here produced ragged, gappy polygon edges once the tile was blitted.
    const float a = ((color >> 24) & 0xff) / 255.0f;
    for (int c = 0; c < 3; ++c) {
        const float src = (color >> (c * 8)) & 0xff;
        const float dst = buf_[off + c];
        const float out = src * a * cov + dst * (1.0f - a * cov);
        buf_[off + c] = static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, out + 0.5f)));
    }
    buf_[off + 3] = 0xff;
}

// Scanline fill using the even-odd rule over one or more rings.  This
// naturally supports holes: an outer ring paired with inner rings produces
// alternating intersections that leave the hole regions unfilled.
//
// Per-pixel coverage is computed from the exact fractional intersection
// positions so polygon edges are antialiased (even/consistent) instead of
// ragged.  The previous conservative floor/ceil fill over-filled every edge
// by up to a pixel, which made boundaries look uneven and blocky.
void TileRasterizer::scanline_fill(const std::vector<const Ring*>& rings,
                                     uint32_t color) {
    if (rings.empty()) return;
    double y_min = kSize, y_max = 0;
    for (const auto* ring : rings) {
        for (const auto& p : *ring) {
            y_min = std::min(y_min, p.y);
            y_max = std::max(y_max, p.y);
        }
    }
    int y0 = std::max(0, static_cast<int>(std::floor(y_min)));
    int y1 = std::min(kSize - 1, static_cast<int>(std::ceil(y_max)));

    for (int y = y0; y <= y1; ++y) {
        const double yy = y + 0.5;
        std::vector<double> xs;
        for (const auto* ring : rings) {
            const size_t n = ring->size();
            for (size_t i = 0, j = n - 1; i < n; j = i++) {
                const Point& a = (*ring)[j];
                const Point& b = (*ring)[i];
                if ((a.y > yy) == (b.y > yy)) continue;
                const double t = (yy - a.y) / (b.y - a.y);
                const double x = a.x + (b.x - a.x) * t;
                xs.push_back(x);
            }
        }
        if (xs.size() < 2) continue;
        std::sort(xs.begin(), xs.end());
        for (size_t i = 0; i + 1 < xs.size(); i += 2) {
            const double x_lo = xs[i];
            const double x_hi = xs[i + 1];
            const int l = static_cast<int>(std::floor(x_lo));
            const int r = static_cast<int>(std::floor(x_hi));
            if (l == r) {
                blend_pixel(l, y, color, x_hi - x_lo);
            } else {
                blend_pixel(l, y, color, static_cast<double>(l + 1) - x_lo);
                for (int x = l + 1; x <= r - 1; ++x) blend_pixel(x, y, color, 1.0f);
                blend_pixel(r, y, color, x_hi - r);
            }
        }
    }
}

void TileRasterizer::draw_area(const Ring& outer, uint32_t color) {
    scanline_fill({&outer}, color);
}

void TileRasterizer::draw_area(const Ring& outer,
                               const std::vector<Ring>& holes,
                               uint32_t color) {
    std::vector<const Ring*> rings = {&outer};
    for (const auto& hole : holes) rings.push_back(&hole);
    scanline_fill(rings, color);
}

void TileRasterizer::thick_line_segment(const Point& a0, const Point& b0,
                                        float width, uint32_t color) {
    Point a = a0, b = b0;
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double len = std::hypot(dx, dy);
    if (len < 1e-6) {
        blend_pixel(static_cast<int>(std::round(a.x)),
                    static_cast<int>(std::round(a.y)), color, 1.0f);
        return;
    }
    const double nx = -dy / len * width * 0.5;
    const double ny =  dx / len * width * 0.5;

    Ring rect;
    rect.push_back({a.x + nx, a.y + ny});
    rect.push_back({b.x + nx, b.y + ny});
    rect.push_back({b.x - nx, b.y - ny});
    rect.push_back({a.x - nx, a.y - ny});
    rect.push_back(rect.front());
    scanline_fill({&rect}, color);
}

void TileRasterizer::draw_line(const Ring& line, float width, uint32_t color) {
    if (line.size() < 2) return;
    // Clip each segment independently.  Clipping the whole polyline at once
    // can join disconnected pieces when a line exits and re-enters the tile,
    // producing spurious diagonal strokes across the tile.
    const double margin = std::max(2.0, width * 0.5 + 1.0);
    for (size_t i = 1; i < line.size(); ++i) {
        Ring seg{line[i-1], line[i]};
        auto clipped = clip_to_rect(seg, kSize, margin, false);
        for (size_t j = 1; j < clipped.size(); ++j) {
            thick_line_segment(clipped[j-1], clipped[j], width, color);
        }
    }
}

}  // namespace mapbake
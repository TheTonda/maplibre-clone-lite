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

void TileRasterizer::set_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= kSize || y < 0 || y >= kSize) return;
    const size_t off = (static_cast<size_t>(y) * kSize + x) * 4;
    buf_[off+0] = color & 0xff;
    buf_[off+1] = (color >> 8) & 0xff;
    buf_[off+2] = (color >> 16) & 0xff;
    buf_[off+3] = (color >> 24) & 0xff;
}

// Simple scanline fill for a polygon ring. Handles convex/concave but not
// self-intersecting rings (acceptable for OSM in MVP).
void TileRasterizer::scanline_fill(const Ring& ring, uint32_t color) {
    if (ring.size() < 3) return;
    double y_min = kSize, y_max = 0;
    for (const auto& p : ring) {
        y_min = std::min(y_min, p.y);
        y_max = std::max(y_max, p.y);
    }
    int y0 = std::max(0, static_cast<int>(std::floor(y_min)));
    int y1 = std::min(kSize - 1, static_cast<int>(std::ceil(y_max)));

    for (int y = y0; y <= y1; ++y) {
        const double yy = y + 0.5;
        std::vector<double> xs;
        const size_t n = ring.size();
        for (size_t i = 0, j = n - 1; i < n; j = i++) {
            const Point& a = ring[j];
            const Point& b = ring[i];
            if ((a.y > yy) == (b.y > yy)) continue;
            const double t = (yy - a.y) / (b.y - a.y);
            const double x = a.x + (b.x - a.x) * t;
            xs.push_back(x);
        }
        std::sort(xs.begin(), xs.end());
        for (size_t i = 0; i + 1 < xs.size(); i += 2) {
            // Conservative rasterization: any pixel touched by the polygon is filled.
            int x0 = static_cast<int>(std::floor(xs[i]));
            int x1 = static_cast<int>(std::ceil(xs[i+1]));
            x0 = std::max(0, x0);
            x1 = std::min(kSize - 1, x1);
            for (int x = x0; x <= x1; ++x) set_pixel(x, y, color);
        }
    }
}

void TileRasterizer::draw_area(const Ring& ring, uint32_t color) {
    scanline_fill(ring, color);
}

void TileRasterizer::thick_line_segment(const Point& a0, const Point& b0,
                                        float width, uint32_t color) {
    Point a = a0, b = b0;
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double len = std::hypot(dx, dy);
    if (len < 1e-6) {
        set_pixel(static_cast<int>(a.x), static_cast<int>(a.y), color);
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
    scanline_fill(rect, color);
}

void TileRasterizer::draw_line(const Ring& line, float width, uint32_t color) {
    if (line.size() < 2) return;
    for (size_t i = 1; i < line.size(); ++i) {
        thick_line_segment(line[i-1], line[i], width, color);
    }
}

}  // namespace mapbake
#pragma once

#include "geometry_clip.h"

#include <cstdint>
#include <vector>

namespace mapbake {

class TileRasterizer {
public:
    static constexpr int kSize = 256;

    TileRasterizer();

    void clear(uint32_t bg = 0xfff5f5f3);
    void draw_area(const Ring& outer, uint32_t color);
    void draw_area(const Ring& outer,
                   const std::vector<Ring>& holes,
                   uint32_t color);
    void draw_line(const Ring& line, float width, uint32_t color);

    const std::vector<uint8_t>& rgba8() const { return buf_; }
    std::vector<uint8_t>& rgba8() { return buf_; }

private:
    std::vector<uint8_t> buf_;  // RGBA8, kSize*kSize*4

    void set_pixel(int x, int y, uint32_t color);
    void scanline_fill(const std::vector<const Ring*>& rings, uint32_t color);
    void thick_line_segment(const Point& a, const Point& b, float width, uint32_t color);
};

}  // namespace mapbake

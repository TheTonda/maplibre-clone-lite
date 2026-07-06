#pragma once

#include <cstdint>
#include <vector>

namespace mapbake {

struct Point { double x, y; };
using Ring = std::vector<Point>;

struct Feature {
    int layer = 0;
    uint32_t color = 0;
    float line_width = 0.0f;  // 0 for areas
    bool is_area = false;
    Ring geometry;                   // outer ring for areas, line for lines
    std::vector<Ring> inner_rings;   // holes for areas (empty for lines)
};

// Clip a ring/line to the axis-aligned rectangle [0..size] x [0..size].
// For areas returns the clipped polygon (may be empty).
// For lines returns clipped line segments concatenated (gaps possible but ok for MVP).
Ring clip_to_rect(const Ring& ring, double size, bool close_result);

// Clip to [0..size] expanded by margin on every side, i.e. [-margin..size+margin].
// Useful to draw anti-aliased or exactly-boundary features consistently across tile edges.
Ring clip_to_rect(const Ring& ring, double size, double margin, bool close_result);

}  // namespace mapbake

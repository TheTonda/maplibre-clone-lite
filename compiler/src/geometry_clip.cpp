#include "geometry_clip.h"

#include <algorithm>
#include <cmath>

namespace mapbake {

static bool inside_left(const Point& p, double /*size*/)   { return p.x >= 0; }
static bool inside_right(const Point& p, double size)      { return p.x <= size; }
static bool inside_bottom(const Point& p, double size)     { return p.y <= size; }  // y down
static bool inside_top(const Point& p, double /*size*/)    { return p.y >= 0; }

static Point intersect(const Point& a, const Point& b,
                       double t, double fixed, bool vertical) {
    if (vertical) {
        double y = a.y + (b.y - a.y) * t;
        return {fixed, y};
    } else {
        double x = a.x + (b.x - a.x) * t;
        return {x, fixed};
    }
}

using InsideFn = bool (*)(const Point&, double);

static Ring clip_edge(const Ring& input,
                      InsideFn inside,
                      int coord, double fixed, double size) {
    Ring output;
    if (input.empty()) return output;
    const size_t n = input.size();
    const bool closed = (input.front().x == input.back().x && input.front().y == input.back().y);
    const size_t count = closed ? n - 1 : n;

    Point s = input.back();
    for (size_t i = 0; i < count; ++i) {
        const Point& e = input[i];
        const bool sin = inside(s, size);
        const bool ein = inside(e, size);
        if (sin && ein) {
            output.push_back(e);
        } else if (sin && !ein) {
            const double ds = (coord == 0) ? (s.x - fixed) : (s.y - fixed);
            const double de = (coord == 0) ? (e.x - fixed) : (e.y - fixed);
            double t = ds / (ds - de);
            output.push_back(intersect(s, e, t, fixed, coord == 0));
        } else if (!sin && ein) {
            const double ds = (coord == 0) ? (s.x - fixed) : (s.y - fixed);
            const double de = (coord == 0) ? (e.x - fixed) : (e.y - fixed);
            double t = ds / (ds - de);
            output.push_back(intersect(s, e, t, fixed, coord == 0));
            output.push_back(e);
        }
        s = e;
    }
    return output;
}

Ring clip_to_rect(const Ring& ring, double size, bool close_result) {
    Ring out = clip_edge(ring, inside_left,   0, 0, size);
    out = clip_edge(out, inside_right,  0, size, size);
    out = clip_edge(out, inside_top,    1, 0, size);
    out = clip_edge(out, inside_bottom, 1, size, size);
    if (close_result && !out.empty() &&
        (out.front().x != out.back().x || out.front().y != out.back().y)) {
        out.push_back(out.front());
    }
    return out;
}

Ring clip_to_rect(const Ring& ring, double size, double margin, bool close_result) {
    if (margin <= 0.0) return clip_to_rect(ring, size, close_result);
    Ring shifted;
    shifted.reserve(ring.size());
    for (const auto& p : ring) shifted.push_back({p.x + margin, p.y + margin});
    Ring clipped = clip_to_rect(shifted, size + 2.0 * margin, close_result);
    for (auto& p : clipped) { p.x -= margin; p.y -= margin; }
    return clipped;
}

}  // namespace mapbake
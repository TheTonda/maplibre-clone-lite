#pragma once

#include "mbtiles_reader.h"

#include <memory>
#include <string>
#include <vector>

namespace maprender {

struct Frame {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> pixels;  // RGBA8, width*height*4
};

struct Context {
    std::unique_ptr<MBTilesReader> reader;
    std::string last_error;

    // Viewport (Phase 2 will fill these in)
    double center_lon = 0.0;
    double center_lat = 0.0;
    int    zoom = 0;
    int    screen_w = 0;
    int    screen_h = 0;
    Frame  frame;
};

}  // namespace maprender
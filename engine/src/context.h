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
    std::unique_ptr<class Renderer> renderer;
    std::string last_error;

    double center_lon = 0.0;
    double center_lat = 0.0;
    int    zoom = 0;
    int    screen_w = 0;
    int    screen_h = 0;
    int    min_zoom = 0;
    int    max_zoom = 0;
    Frame  frame;
    long   view_gen = 0;  // bumped whenever view fields change
};

}  // namespace maprender
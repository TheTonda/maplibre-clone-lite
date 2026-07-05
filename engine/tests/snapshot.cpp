// Phase 2 snapshot CLI: open an .mbtiles, render the center view, save PNG.
// Useful for eyeballing manual tweaks before wiring FLTK.

#include "maprender/c_api.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 5) {
        std::fprintf(stderr,
            "usage: %s <in.mbtiles> <lon> <lat> <zoom> <w> <h> [out.png]\n",
            argv[0]);
        return 2;
    }
    const char* path = argv[1];
    const double lon = std::atof(argv[2]);
    const double lat = std::atof(argv[3]);
    const int z = std::atoi(argv[4]);
    const int w = std::atoi(argv[5]);
    const int h = std::atoi(argv[6]);
    const std::string out = (argc >= 8) ? argv[7] : "snapshot.png";

    MR_Context* ctx = mr_open(path);
    if (!ctx) {
        std::fprintf(stderr, "mr_open failed: %s\n", mr_last_error(nullptr));
        return 1;
    }
    mr_set_view(ctx, lon, lat, z, w, h);
    const MR_Frame* f = mr_render(ctx);
    if (!f) {
        std::fprintf(stderr, "mr_render failed: %s\n", mr_last_error(ctx));
        mr_close(ctx);
        return 1;
    }
    const unsigned char* px = mr_frame_pixels(f);
    if (!stbi_write_png(out.c_str(), w, h, 4, px, w * 4)) {
        std::fprintf(stderr, "write failed: %s\n", out.c_str());
        mr_close(ctx);
        return 1;
    }
    std::printf("rendered %dx%d -> %s\n", w, h, out.c_str());
    mr_close(ctx);
    return 0;
}
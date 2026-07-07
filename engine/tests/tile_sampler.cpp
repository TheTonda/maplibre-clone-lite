// Renders a grid of adjacent multi-tile views from a WebP-raster .mbtiles.
// Usage: tile_sampler <in.mbtiles> <out_dir>

#include "maprender/c_api.h"
#include "maprender/mercator.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

struct SamplePlan {
    int zoom;
    int view_size;  // square viewport in pixels
    int grid;       // render grid x grid adjacent views
};

static const SamplePlan kPlans[] = {
    {8,  512, 1},
    {10, 512, 1},
    {12, 512, 2},
    {14, 512, 2},
    {16, 512, 3},
    {17, 768, 3},
    {18, 768, 4},
    {19, 768, 4},
    {20, 768, 4},
};

static std::string make_path(const std::string& dir, int z, int r, int c) {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%s/z%02d_r%d_c%d.png", dir.c_str(), z, r, c);
    return buf;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <in.mbtiles> <out_dir>\n", argv[0]);
        return 2;
    }
    const char* path = argv[1];
    const std::string out_dir = argv[2];

    MR_Context* ctx = mr_open(path);
    if (!ctx) {
        std::fprintf(stderr, "mr_open failed: %s\n", mr_last_error(nullptr));
        return 1;
    }

    const double center_lon = 77.22;
    const double center_lat = 28.62;

    int total = 0;
    for (const auto& plan : kPlans) total += plan.grid * plan.grid;
    int done = 0;

    for (const auto& plan : kPlans) {
        const int z = plan.zoom;
        const int size = plan.view_size;
        const int g = plan.grid;

        const double cx = maprender::lon_to_world_x(center_lon, z);
        const double cy = maprender::lat_to_world_y(center_lat, z);
        const int center_tx = static_cast<int>(std::floor(cx / maprender::kTileSize));
        const int center_ty = static_cast<int>(std::floor(cy / maprender::kTileSize));

        // Step in tile units; view spans size/256 tiles, step by roughly that.
        const int step_tiles = std::max(1, size / maprender::kTileSize);

        for (int r = 0; r < g; ++r) {
            for (int c = 0; c < g; ++c) {
                const int off_r = r - g / 2;
                const int off_c = c - g / 2;
                const int tx = center_tx + off_c * step_tiles;
                const int ty = center_ty + off_r * step_tiles;

                const double wx = (tx + 0.5) * maprender::kTileSize;
                const double wy = (ty + 0.5) * maprender::kTileSize;
                const double lon = maprender::world_x_to_lon(wx, z);
                const double lat = maprender::world_y_to_lat(wy, z);

                mr_set_view(ctx, lon, lat, z, size, size);
                const MR_Frame* f = mr_render(ctx);
                if (!f) {
                    std::fprintf(stderr, "mr_render failed at z=%d r=%d c=%d: %s\n",
                                 z, r, c, mr_last_error(ctx));
                    continue;
                }
                const std::string out = make_path(out_dir, z, r, c);
                if (!stbi_write_png(out.c_str(), size, size, 4,
                                    mr_frame_pixels(f), size * 4)) {
                    std::fprintf(stderr, "write failed: %s\n", out.c_str());
                }
                ++done;
                if (done % 10 == 0) {
                    std::fprintf(stderr, "  %d/%d rendered\n", done, total);
                }
            }
        }
    }

    std::printf("wrote %d samples to %s\n", done, out_dir.c_str());
    mr_close(ctx);
    return 0;
}

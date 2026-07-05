// Smoke test for the compiler's rasterizer: draw a few shapes and save as PNG.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include "geometry_clip.h"
#include "tile_rasterizer.h"

#include <vector>

int main() {
    using namespace mapbake;
    TileRasterizer r;
    r.clear(0xffffffff);

    Ring poly = {{50,50},{200,50},{200,200},{50,200},{50,50}};
    r.draw_area(poly, 0xffff0000);

    Ring line = {{30,30},{220,220}};
    r.draw_line(line, 4.0f, 0xff0000ff);

    stbi_write_png("/tmp/opencode/rasterizer_smoke.png", 256, 256, 4,
                   r.rgba8().data(), 256*4);
    return 0;
}
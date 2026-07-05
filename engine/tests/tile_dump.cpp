// Phase 1 smoke: open an .mbtiles via the engine ABI (validates sqlite +
// metadata path), then decode one WebP tile directly and dump it as a PNG.
//
// The full ABI for reading individual tiles is deferred to Phase 2 (via a
// renderer frame); here we exercise libwebp + sqlite directly to confirm
// the fixture format round-trips.

#include "maprender/c_api.h"

#include <sqlite3.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>
#include <webp/decode.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 5) {
        std::fprintf(stderr,
            "usage: %s <in.mbtiles> <z> <x> <slippy_y> [out.png]\n",
            argv[0]);
        return 2;
    }
    const char* path = argv[1];
    const int z = std::atoi(argv[2]);
    const int x = std::atoi(argv[3]);
    const int slippy_y = std::atoi(argv[4]);
    const std::string out = (argc >= 6) ? argv[5] :
        ("tile_" + std::to_string(z) + "_" + std::to_string(x) +
         "_" + std::to_string(slippy_y) + ".png");

    MR_Context* ctx = mr_open(path);
    if (!ctx) {
        std::fprintf(stderr, "mr_open failed: %s\n", mr_last_error(nullptr));
        return 1;
    }
    std::printf("opened %s  minzoom=%d maxzoom=%d\n",
                path, mr_min_zoom(ctx), mr_max_zoom(ctx));
    double w, s, e, n;
    mr_bounds(ctx, &w, &s, &e, &n);
    std::printf("bounds w=%.4f s=%.4f e=%.4f n=%.4f\n", w, s, e, n);
    mr_close(ctx);

    sqlite3* db = nullptr;
    if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        std::fprintf(stderr, "sqlite open failed\n");
        return 1;
    }
    const int tms_row = (1 << z) - 1 - slippy_y;
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db,
        "SELECT tile_data FROM tiles WHERE zoom_level=? AND tile_column=? AND tile_row=?;",
        -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, z);
    sqlite3_bind_int(stmt, 2, x);
    sqlite3_bind_int(stmt, 3, tms_row);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        std::fprintf(stderr, "no tile z=%d x=%d tms_row=%d\n", z, x, tms_row);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return 1;
    }
    const void* blob = sqlite3_column_blob(stmt, 0);
    int bytes = sqlite3_column_bytes(stmt, 0);
    std::vector<unsigned char> webp(
        static_cast<const unsigned char*>(blob),
        static_cast<const unsigned char*>(blob) + bytes);
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    int wpx = 0, hpx = 0;
    if (!WebPGetInfo(webp.data(), webp.size(), &wpx, &hpx)) {
        std::fprintf(stderr, "not a webp blob\n");
        return 1;
    }
    std::vector<unsigned char> rgba(static_cast<size_t>(wpx) * hpx * 4);
    if (!WebPDecodeRGBAInto(webp.data(), webp.size(), rgba.data(), rgba.size(), wpx * 4)) {
        std::fprintf(stderr, "webp decode failed\n");
        return 1;
    }
    const int stride = wpx * 4;
    if (!stbi_write_png(out.c_str(), wpx, hpx, 4, rgba.data(), stride)) {
        std::fprintf(stderr, "png write failed: %s\n", out.c_str());
        return 1;
    }
    std::printf("decoded %dx%d -> %s (%d bytes webp)\n", wpx, hpx, out.c_str(), bytes);
    return 0;
}
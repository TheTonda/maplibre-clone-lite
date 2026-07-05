// Generates a tiny WebP-raster .mbtiles test fixture covering the four
// tiles at z=8 around New Delhi: (136, 84), (136,85), (137,84), (137,85).
// Each tile is a 256x256 solid-colour WebP so the engine can decode it.

#include <sqlite3.h>
#include <webp/encode.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#ifndef MR_FIXTURE_OUT
#error "MR_FIXTURE_OUT must be defined"
#endif

namespace {

constexpr int kTile = 256;
constexpr int kBands = 4;

struct Color { unsigned char r, g, b; };

// One solid colour per tile so we can eyeball that TMS flip works.
const Color kColors[2][2] = {
    {{230, 120, 120}, {120, 200, 230}},  // row 0 slippy: red and cyan
    {{120, 230, 130}, {230, 230, 120}},  // row 1 slippy: green and yellow
};

bool encode_tile_solidwebp(const Color& c, std::vector<unsigned char>& out) {
    std::vector<unsigned char> rgba(kTile * kTile * kBands);
    for (size_t i = 0; i < rgba.size(); i += kBands) {
        rgba[i + 0] = c.r;
        rgba[i + 1] = c.g;
        rgba[i + 2] = c.b;
        rgba[i + 3] = 255;
    }
    uint8_t* webp = nullptr;
    size_t webp_size = WebPEncodeRGBA(rgba.data(), kTile, kTile, kTile * kBands,
                                      0.85f, &webp);
    if (!webp || webp_size == 0) return false;
    out.assign(webp, webp + webp_size);
    WebPFree(webp);
    return true;
}

int run() {
    sqlite3* db = nullptr;
    const std::string path = std::string(MR_FIXTURE_OUT) + "/sample.webp.mbtiles";
    remove(path.c_str());
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        std::fprintf(stderr, "cannot open %s\n", path.c_str());
        return 1;
    }
    char* err = nullptr;
    if (sqlite3_exec(db,
            "CREATE TABLE IF NOT EXISTS metadata (name TEXT, value TEXT);"
            "CREATE TABLE IF NOT EXISTS tiles ("
            "  zoom_level INTEGER, tile_column INTEGER, tile_row INTEGER,"
            "  tile_data BLOB, PRIMARY KEY (zoom_level, tile_column, tile_row));",
            nullptr, nullptr, &err) != SQLITE_OK) {
        std::fprintf(stderr, "schema: %s\n", err);
        sqlite3_free(err);
        return 1;
    }
    sqlite3_stmt* meta_stmt = nullptr;
    sqlite3_prepare_v2(db, "INSERT INTO metadata (name,value) VALUES (?,?);",
                      -1, &meta_stmt, nullptr);
    auto insert_meta = [&](const std::string& k, const std::string& v) {
        sqlite3_bind_text(meta_stmt, 1, k.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(meta_stmt, 2, v.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(meta_stmt);
        sqlite3_reset(meta_stmt);
    };
    insert_meta("name", "test");
    insert_meta("format", "webp");
    insert_meta("minzoom", "8");
    insert_meta("maxzoom", "8");
    insert_meta("bounds", "76.692,28.183,77.733,28.969");
    insert_meta("scheme", "tms");
    insert_meta("type", "baselayer");
    sqlite3_finalize(meta_stmt);

    sqlite3_stmt* ti = nullptr;
    sqlite3_prepare_v2(db,
        "INSERT INTO tiles (zoom_level,tile_column,tile_row,tile_data) VALUES (?,?,?,?);",
        -1, &ti, nullptr);

    const int z = 8;
    const int tmsflip = (1 << z) - 1;  // slippy_y -> tms_row
    for (int sx = 0; sx < 2; ++sx) {
        for (int sy = 0; sy < 2; ++sy) {
            const int x = 136 + sx;
            const int slippy_y = 84 + sy;
            const int tms_row = tmsflip - slippy_y;
            std::vector<unsigned char> blob;
            if (!encode_tile_solidwebp(kColors[sy][sx], blob)) {
                std::fprintf(stderr, "webp encode failed\n");
                return 1;
            }
            sqlite3_bind_int(ti, 1, z);
            sqlite3_bind_int(ti, 2, x);
            sqlite3_bind_int(ti, 3, tms_row);
            sqlite3_bind_blob(ti, 4, blob.data(), static_cast<int>(blob.size()),
                              SQLITE_TRANSIENT);
            if (sqlite3_step(ti) != SQLITE_DONE) {
                std::fprintf(stderr, "insert failed: %s\n", sqlite3_errmsg(db));
                return 1;
            }
            sqlite3_reset(ti);
        }
    }
    sqlite3_finalize(ti);
    sqlite3_close(db);
    std::printf("wrote %s\n", path.c_str());
    return 0;
}

}  // namespace

int main() { return run(); }
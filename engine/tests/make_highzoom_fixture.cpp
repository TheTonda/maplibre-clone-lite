// Generates a tiny WebP-raster .mbtiles test fixture at z=18.
// Four solid-colour tiles arranged in a 2x2 grid so we can verify the
// engine's high-zoom TMS flip, tile range, and blitting.

#include <sqlite3.h>
#include <webp/encode.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "viewport.h"

#ifndef MR_FIXTURE_OUT
#error "MR_FIXTURE_OUT must be defined"
#endif

namespace {

constexpr int kTile = 256;
constexpr int kBands = 4;
constexpr int kZ = 18;

struct Color { unsigned char r, g, b; };

bool encode_solid(Color c, std::vector<unsigned char>& out) {
    std::vector<unsigned char> rgba(kTile * kTile * kBands);
    for (size_t i = 0; i < rgba.size(); i += kBands) {
        rgba[i+0]=c.r; rgba[i+1]=c.g; rgba[i+2]=c.b; rgba[i+3]=255;
    }
    uint8_t* webp = nullptr;
    size_t n = WebPEncodeRGBA(rgba.data(), kTile, kTile, kTile*kBands, 0.85f, &webp);
    if (!webp || n == 0) return false;
    out.assign(webp, webp + n);
    WebPFree(webp);
    return true;
}

int run() {
    // Pick a concrete lon/lat in central New Delhi and compute its tile.
    const double lon = 77.22;
    const double lat = 28.62;
    const int x0 = static_cast<int>(std::floor(maprender::lon_to_world_x(lon, kZ) / kTile));
    const int y0 = static_cast<int>(std::floor(maprender::lat_to_world_y(lat, kZ) / kTile));

    sqlite3* db = nullptr;
    const std::string path = std::string(MR_FIXTURE_OUT) + "/highzoom.webp.mbtiles";
    remove(path.c_str());
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        std::fprintf(stderr, "open %s\n", path.c_str());
        return 1;
    }
    char* err = nullptr;
    if (sqlite3_exec(db,
            "CREATE TABLE metadata (name TEXT, value TEXT);"
            "CREATE TABLE tiles ("
            "  zoom_level INTEGER, tile_column INTEGER, tile_row INTEGER,"
            "  tile_data BLOB, PRIMARY KEY (zoom_level, tile_column, tile_row));",
            nullptr, nullptr, &err) != SQLITE_OK) {
        std::fprintf(stderr, "schema: %s\n", err); sqlite3_free(err); return 1;
    }
    sqlite3_stmt* ms = nullptr;
    sqlite3_prepare_v2(db, "INSERT INTO metadata (name,value) VALUES (?,?);", -1, &ms, nullptr);
    auto meta = [&](const std::string& k, const std::string& v) {
        sqlite3_bind_text(ms,1,k.c_str(),-1,SQLITE_TRANSIENT);
        sqlite3_bind_text(ms,2,v.c_str(),-1,SQLITE_TRANSIENT);
        sqlite3_step(ms); sqlite3_reset(ms);
    };
    meta("name","highzoom-test"); meta("format","webp");
    meta("minzoom","18"); meta("maxzoom","18");
    meta("bounds","77.20000,28.60000,77.24000,28.64000");
    meta("scheme","tms"); meta("type","baselayer");
    sqlite3_finalize(ms);

    sqlite3_stmt* ti = nullptr;
    sqlite3_prepare_v2(db,
        "INSERT INTO tiles (zoom_level,tile_column,tile_row,tile_data) VALUES (?,?,?,?);",
        -1, &ti, nullptr);

    const int tms_flip = static_cast<int>((1u << kZ) - 1);
    int n_tiles = 0;
    const Color palette[4] = {
        {230,120,120},  // top-left
        {120,200,230},  // top-right
        {120,230,130},  // bottom-left
        {230,230,120},  // bottom-right
    };
    for (int dx = 0; dx < 2; ++dx) {
        for (int dy = 0; dy < 2; ++dy) {
            const int x = x0 + dx;
            const int slippy_y = y0 + dy;
            const int tms_row = tms_flip - slippy_y;
            std::vector<unsigned char> blob;
            const Color c = palette[dy * 2 + dx];
            if (!encode_solid(c, blob)) { std::fprintf(stderr,"encode\n"); return 1; }
            sqlite3_bind_int(ti,1,kZ);
            sqlite3_bind_int(ti,2,x);
            sqlite3_bind_int(ti,3,tms_row);
            sqlite3_bind_blob(ti,4,blob.data(),static_cast<int>(blob.size()),SQLITE_TRANSIENT);
            if (sqlite3_step(ti)!=SQLITE_DONE) {
                std::fprintf(stderr,"insert: %s\n", sqlite3_errmsg(db)); return 1;
            }
            sqlite3_reset(ti);
            ++n_tiles;
        }
    }
    sqlite3_finalize(ti);
    sqlite3_close(db);
    std::printf("wrote %s  tiles=%d  z=%d  x=[%d..%d] slippy_y=[%d..%d]\n",
                path.c_str(), n_tiles, kZ, x0, x0+1, y0, y0+1);
    return 0;
}

}  // namespace

int main() { return run(); }

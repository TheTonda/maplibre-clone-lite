#include "mbtiles_reader.h"

#include <sqlite3.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace maprender {

MBTilesReader::MBTilesReader(const std::string& path) : path_(path) {}

MBTilesReader::~MBTilesReader() { close(); }

bool MBTilesReader::open() {
    if (sqlite3_open_v2(path_.c_str(), &db_, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        db_ = nullptr;
        return false;
    }
    load_metadata();
    return true;
}

void MBTilesReader::close() {
    if (db_) {
        sqlite3_close_v2(db_);
        db_ = nullptr;
    }
}

static std::string meta_get(sqlite3* db, const std::string& key) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT value FROM metadata WHERE name=?;", -1, &stmt, nullptr) != SQLITE_OK)
        return {};
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    std::string value;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* v = sqlite3_column_text(stmt, 0);
        if (v) value = reinterpret_cast<const char*>(v);
    }
    sqlite3_finalize(stmt);
    return value;
}

void MBTilesReader::load_metadata() {
    min_zoom_ = std::atoi(meta_get(db_, "minzoom").c_str());
    max_zoom_ = std::atoi(meta_get(db_, "maxzoom").c_str());

    std::string bstr = meta_get(db_, "bounds");  // w,s,e,n
    if (!bstr.empty()) {
        std::replace(bstr.begin(), bstr.end(), ',', ' ');
        std::istringstream iss(bstr);
        iss >> bounds_[0] >> bounds_[1] >> bounds_[2] >> bounds_[3];
    }
}

void MBTilesReader::bounds(double& w, double& s, double& e, double& n) const {
    w = bounds_[0]; s = bounds_[1]; e = bounds_[2]; n = bounds_[3];
}

bool MBTilesReader::read_tile(int z, int x, int slippy_y, std::vector<unsigned char>& out) {
    if (!db_) return false;

    const int tms_row = (1 << z) - 1 - slippy_y;  // MBTiles stores TMS rows
    sqlite3_stmt* stmt = nullptr;
    static const char* kSql =
        "SELECT tile_data FROM tiles WHERE zoom_level=? AND tile_column=? AND tile_row=?;";
    if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, z);
    sqlite3_bind_int(stmt, 2, x);
    sqlite3_bind_int(stmt, 3, tms_row);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const void* blob = sqlite3_column_blob(stmt, 0);
        int bytes = sqlite3_column_bytes(stmt, 0);
        if (blob && bytes > 0) {
            out.resize(static_cast<size_t>(bytes));
            std::memcpy(out.data(), blob, static_cast<size_t>(bytes));
            found = true;
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

}  // namespace maprender
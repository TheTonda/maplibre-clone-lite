#include "mbtiles_writer.h"

#include <cstdio>
#include <string>

namespace mapbake {

MBTilesWriter::MBTilesWriter(const std::string& path) : path_(path) {}

MBTilesWriter::~MBTilesWriter() { close(); }

bool MBTilesWriter::open(int min_z, int max_z,
                         const std::string& bounds, const std::string& name) {
    std::remove(path_.c_str());
    if (sqlite3_open(path_.c_str(), &db_) != SQLITE_OK) {
        db_ = nullptr;
        return false;
    }
    char* err = nullptr;
    sqlite3_exec(db_,
        "PRAGMA synchronous=OFF;"
        "PRAGMA journal_mode=OFF;"
        "PRAGMA locking_mode=EXCLUSIVE;"
        "PRAGMA cache_size=-1048576;",
        nullptr, nullptr, &err);
    if (err) { sqlite3_free(err); err = nullptr; }

    if (sqlite3_exec(db_,
        "CREATE TABLE metadata (name TEXT, value TEXT);"
        "CREATE TABLE tiles ("
        "  zoom_level INTEGER, tile_column INTEGER, tile_row INTEGER,"
        "  tile_data BLOB, PRIMARY KEY (zoom_level, tile_column, tile_row));",
        nullptr, nullptr, &err) != SQLITE_OK) {
        std::fprintf(stderr, "mbtiles schema: %s\n", err);
        sqlite3_free(err);
        return false;
    }

    auto meta = [&](const char* k, const char* v) {
        sqlite3_stmt* s = nullptr;
        sqlite3_prepare_v2(db_, "INSERT INTO metadata (name,value) VALUES (?,?);", -1, &s, nullptr);
        sqlite3_bind_text(s, 1, k, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(s, 2, v, -1, SQLITE_TRANSIENT);
        sqlite3_step(s);
        sqlite3_finalize(s);
    };
    meta("name", name.c_str());
    meta("format", "webp");
    meta("minzoom", std::to_string(min_z).c_str());
    meta("maxzoom", std::to_string(max_z).c_str());
    meta("bounds", bounds.c_str());
    meta("scheme", "tms");
    meta("type", "baselayer");

    sqlite3_prepare_v2(db_,
        "INSERT INTO tiles (zoom_level,tile_column,tile_row,tile_data) VALUES (?,?,?,?);",
        -1, &insert_, nullptr);
    return true;
}

bool MBTilesWriter::write_tile(int z, int tms_x, int tms_y, const std::vector<uint8_t>& blob) {
    if (!insert_) return false;
    sqlite3_bind_int(insert_, 1, z);
    sqlite3_bind_int(insert_, 2, tms_x);
    sqlite3_bind_int(insert_, 3, tms_y);
    sqlite3_bind_blob(insert_, 4, blob.data(), static_cast<int>(blob.size()), SQLITE_TRANSIENT);
    bool ok = sqlite3_step(insert_) == SQLITE_DONE;
    sqlite3_reset(insert_);
    return ok;
}

bool MBTilesWriter::begin_batch() {
    if (!db_) return false;
    char* err = nullptr;
    bool ok = sqlite3_exec(db_, "BEGIN;", nullptr, nullptr, &err) == SQLITE_OK;
    if (err) sqlite3_free(err);
    return ok;
}

bool MBTilesWriter::end_batch() {
    if (!db_) return false;
    char* err = nullptr;
    bool ok = sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &err) == SQLITE_OK;
    if (err) sqlite3_free(err);
    return ok;
}

bool MBTilesWriter::close() {
    if (insert_) { sqlite3_finalize(insert_); insert_ = nullptr; }
    if (db_) { sqlite3_close(db_); db_ = nullptr; }
    return true;
}

}  // namespace mapbake
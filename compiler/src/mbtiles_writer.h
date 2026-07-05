#pragma once

#include <sqlite3.h>

#include <cstdint>
#include <string>
#include <vector>

namespace mapbake {

class MBTilesWriter {
public:
    explicit MBTilesWriter(const std::string& path);
    ~MBTilesWriter();
    MBTilesWriter(const MBTilesWriter&) = delete;
    MBTilesWriter& operator=(const MBTilesWriter&) = delete;

    bool open(int min_z, int max_z, const std::string& bounds, const std::string& name);
    bool write_tile(int z, int tms_x, int tms_y, const std::vector<uint8_t>& blob);
    bool close();

private:
    std::string path_;
    struct sqlite3* db_ = nullptr;
    struct sqlite3_stmt* insert_ = nullptr;
};

}  // namespace mapbake
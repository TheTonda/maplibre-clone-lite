#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct sqlite3;

namespace maprender {

class MBTilesReader {
public:
    explicit MBTilesReader(const std::string& path);
    ~MBTilesReader();
    MBTilesReader(const MBTilesReader&) = delete;
    MBTilesReader& operator=(const MBTilesReader&) = delete;

    bool open();
    void close();

    int  min_zoom() const { return min_zoom_; }
    int  max_zoom() const { return max_zoom_; }
    void bounds(double& w, double& s, double& e, double& n) const;

    // Reads a tile blob (gzip/webp bytes) at slippy (z,x,y).
    // Returns false if the tile does not exist.
    bool read_tile(int z, int x, int slippy_y, std::vector<unsigned char>& out);

private:
    std::string path_;
    sqlite3* db_ = nullptr;
    int   min_zoom_ = 0;
    int   max_zoom_ = 0;
    double bounds_[4] = {0, 0, 0, 0};

    void load_metadata();
};

}  // namespace maprender
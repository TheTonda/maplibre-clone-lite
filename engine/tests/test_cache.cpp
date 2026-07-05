#include "mbtiles_reader.h"
#include "tile_cache.h"

#include <gtest/gtest.h>

namespace {
const char* kFixture = "engine/tests/fixtures/sample.webp.mbtiles";
// Coordinates written by make_fixture (New Delhi bounds at z=8).
constexpr int kZ = 8;
constexpr int kX0 = 182;
constexpr int kY0 = 106;  // slippy row, topmost fixture tile
}  // namespace

using maprender::MBTilesReader;
using maprender::TileCache;

TEST(TileCache, HitsAndMisses) {
    MBTilesReader r(kFixture);
    ASSERT_TRUE(r.open()) << "open " << kFixture;
    TileCache cache(r, 8 * 1024 * 1024);
    const auto* a = cache.get(kZ, kX0, kY0);
    ASSERT_NE(a, nullptr);
    ASSERT_EQ(a->w, 256);
    ASSERT_EQ(a->h, 256);
    EXPECT_EQ(cache.bytes_used(), 256u * 256 * 4);

    const auto* b = cache.get(kZ, kX0, kY0);  // hit
    EXPECT_EQ(b, a);

    const auto* miss = cache.get(kZ, 0, 0);  // missing tile -> null
    EXPECT_EQ(miss, nullptr);
}

TEST(TileCache, LruEviction) {
    MBTilesReader r(kFixture);
    ASSERT_TRUE(r.open());
    // 256*256*4 = 256 KiB per tile. 512 KiB budget fits exactly 2.
    TileCache cache(r, 512 * 1024);
    const auto* t1 = cache.get(kZ, kX0,     kY0);     ASSERT_NE(t1, nullptr);
    const auto* t2 = cache.get(kZ, kX0+1,   kY0);     ASSERT_NE(t2, nullptr);
    EXPECT_EQ(cache.bytes_used(), 512u * 1024);
    const auto* t3 = cache.get(kZ, kX0,     kY0+1);   ASSERT_NE(t3, nullptr);
    EXPECT_EQ(cache.bytes_used(), 512u * 1024);
    // t1 was LRU and evicted; re-fetch returns a different pointer.
    const auto* t1b = cache.get(kZ, kX0, kY0);
    EXPECT_NE(t1b, t1);
}
// Tile cache unit tests — Task 15
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <map_renderer/tile_cache.h>

using namespace map_renderer;

// ── Basic operations ─────────────────────────────────────────────────

TEST(TileCacheTest, DefaultCapacity) {
    TileCache cache;
    EXPECT_EQ(cache.capacity(), 64u);
    EXPECT_EQ(cache.size(), 0u);
}

TEST(TileCacheTest, CustomCapacity) {
    TileCache cache(128);
    EXPECT_EQ(cache.capacity(), 128u);
}

TEST(TileCacheTest, PutAndGet) {
    TileCache cache;
    auto tile = std::make_shared<TileData>();
    tile->id = TileId{12, 100, 200};
    cache.put({12, 100, 200}, tile);

    EXPECT_EQ(cache.size(), 1u);
    auto result = cache.get({12, 100, 200});
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->id.z, 12u);
}

TEST(TileCacheTest, GetMissingReturnsNull) {
    TileCache cache;
    auto result = cache.get({12, 999, 999});
    EXPECT_EQ(result, nullptr);
}

TEST(TileCacheTest, GetMarksRecent) {
    TileCache cache(3);
    auto t1 = std::make_shared<TileData>(); t1->id = {8, 0, 0};
    auto t2 = std::make_shared<TileData>(); t2->id = {8, 0, 1};
    auto t3 = std::make_shared<TileData>(); t3->id = {8, 0, 2};

    cache.put({8, 0, 0}, t1);
    cache.put({8, 0, 1}, t2);
    cache.put({8, 0, 2}, t3);

    // Access oldest, making it most recent
    cache.get({8, 0, 0});

    // Now add a 4th tile — should evict the LRU which is now {8,0,1}
    auto t4 = std::make_shared<TileData>(); t4->id = {8, 0, 3};
    cache.put({8, 0, 3}, t4);

    EXPECT_EQ(cache.size(), 3u);
    EXPECT_EQ(cache.get({8, 0, 1}), nullptr);  // should be evicted
    EXPECT_NE(cache.get({8, 0, 0}), nullptr);
    EXPECT_NE(cache.get({8, 0, 2}), nullptr);
    EXPECT_NE(cache.get({8, 0, 3}), nullptr);
}

// ── Eviction ─────────────────────────────────────────────────────────

TEST(TileCacheTest, EvictsWhenFull) {
    TileCache cache(2);
    auto t1 = std::make_shared<TileData>(); t1->id = {8, 0, 0};
    auto t2 = std::make_shared<TileData>(); t2->id = {8, 0, 1};
    auto t3 = std::make_shared<TileData>(); t3->id = {8, 0, 2};

    cache.put({8, 0, 0}, t1);
    cache.put({8, 0, 1}, t2);
    cache.put({8, 0, 2}, t3);  // should evict {8,0,0}

    EXPECT_EQ(cache.size(), 2u);
    EXPECT_EQ(cache.get({8, 0, 0}), nullptr);  // evicted
}

TEST(TileCacheTest, EvictionCallbackFires) {
    TileCache cache(1);
    int callback_count = 0;
    TileId evicted_id{};
    cache.set_eviction_callback(
        [&](const TileId& id, TileData&) {
            ++callback_count;
            evicted_id = id;
        });

    auto t1 = std::make_shared<TileData>(); t1->id = {8, 0, 0};
    auto t2 = std::make_shared<TileData>(); t2->id = {8, 0, 1};

    cache.put({8, 0, 0}, t1);
    cache.put({8, 0, 1}, t2);  // evicts {8,0,0}

    EXPECT_EQ(callback_count, 1);
    EXPECT_EQ(evicted_id.z, 8u);
    EXPECT_EQ(evicted_id.x, 0u);
    EXPECT_EQ(evicted_id.y, 0u);
}

// ── drain_recent_inserts ─────────────────────────────────────────────

TEST(TileCacheTest, DrainRecentInserts) {
    TileCache cache;
    auto t1 = std::make_shared<TileData>(); t1->id = {8, 0, 0};
    auto t2 = std::make_shared<TileData>(); t2->id = {8, 0, 1};

    cache.put({8, 0, 0}, t1);
    cache.put({8, 0, 1}, t2);

    auto recent = cache.drain_recent_inserts();
    EXPECT_EQ(recent.size(), 2u);

    // Second drain should be empty
    recent = cache.drain_recent_inserts();
    EXPECT_TRUE(recent.empty());
}

// ── Invalidate ───────────────────────────────────────────────────────

TEST(TileCacheTest, InvalidateRemovesTile) {
    TileCache cache;
    auto tile = std::make_shared<TileData>();
    cache.put({12, 5, 10}, tile);
    EXPECT_EQ(cache.size(), 1u);

    cache.invalidate({12, 5, 10});
    EXPECT_EQ(cache.size(), 0u);
    EXPECT_EQ(cache.get({12, 5, 10}), nullptr);
}

TEST(TileCacheTest, InvalidateFiresCallback) {
    TileCache cache;
    int cb_count = 0;
    cache.set_eviction_callback([&](const TileId&, TileData&) { ++cb_count; });

    auto tile = std::make_shared<TileData>();
    cache.put({12, 5, 10}, tile);
    cache.invalidate({12, 5, 10});

    EXPECT_EQ(cb_count, 1);
}

TEST(TileCacheTest, InvalidateMissingDoesNothing) {
    TileCache cache;
    cache.invalidate({99, 99, 99});  // should not crash
    SUCCEED();
}

// ── Thread safety ────────────────────────────────────────────────────

TEST(TileCacheTest, ConcurrentPutAndGet) {
    TileCache cache(64);
    std::vector<std::thread> threads;

    // Multiple threads putting tiles
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([&cache, i]() {
            for (int j = 0; j < 100; ++j) {
                auto tile = std::make_shared<TileData>();
                TileId id{static_cast<uint32_t>(i),
                          static_cast<uint32_t>(j), 0};
                tile->id = id;
                cache.put(id, tile);
                cache.get(id);
            }
        });
    }

    for (auto& t : threads) t.join();

    // Cache should have entries (some may have been evicted)
    EXPECT_GT(cache.size(), 0u);
    EXPECT_LE(cache.size(), 64u);
}


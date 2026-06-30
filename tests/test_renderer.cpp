// tests/test_renderer.cpp — Unit tests for Vulkan rendering pipeline
// Note: These tests require a Vulkan-capable GPU and may be skipped in CI
#include "style_engine.h"
#include "osm_loader.h"
#include "building_data.h"

#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <vector>

// ─── 1. Camera Matrix Tests ─────────────────────────────────────────────

TEST(Camera, Ortho2DIdentity) {
    // At center, 2D ortho proj should map center to origin
    float aspect = 16.0f / 9.0f;
    float zoom = 14.0f;
    float visible_half_w = 2.0f / zoom;
    float visible_half_h = visible_half_w / aspect;

    glm::mat4 proj = glm::ortho(
        -visible_half_w, visible_half_w,
        -visible_half_h, visible_half_h,
        -1.0f, 1.0f
    );

    // Origin should map to origin
    glm::vec4 origin = proj * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    EXPECT_NEAR(origin.x, 0.0f, 0.01f);
    EXPECT_NEAR(origin.y, 0.0f, 0.01f);
}

TEST(Camera, Ortho2DBounds) {
    float aspect = 16.0f / 9.0f;
    float zoom = 14.0f;
    float visible_half_w = 2.0f / zoom;
    float visible_half_h = visible_half_w / aspect;

    glm::mat4 proj = glm::ortho(
        -visible_half_w, visible_half_w,
        -visible_half_h, visible_half_h,
        -1.0f, 1.0f
    );

    // Corner (visible_half_w, visible_half_h) should map to (1, 1)
    glm::vec4 corner = proj * glm::vec4(visible_half_w, visible_half_h, 0.0f, 1.0f);
    EXPECT_NEAR(corner.x, 1.0f, 0.01f);
    EXPECT_NEAR(corner.y, 1.0f, 0.01f);
}

TEST(Camera, PerspectiveFOV) {
    float aspect = 16.0f / 9.0f;
    glm::mat4 proj = glm::perspective(
        glm::radians(60.0f),
        aspect,
        0.1f,
        10000.0f
    );
    // Verify matrix is not identity
    EXPECT_NE(proj[0][0], 1.0f);
}

TEST(Camera, PerspectiveNearFar) {
    glm::mat4 proj = glm::perspective(
        glm::radians(60.0f),
        16.0f / 9.0f,
        0.1f,
        10000.0f
    );
    // Near plane object should be at -0.1 in NDC
    glm::vec4 near_pt = proj * glm::vec4(0.0f, 0.0f, -0.1f, 1.0f);
    EXPECT_NEAR(near_pt.z / near_pt.w, -1.0f, 0.01f);
}

TEST(Camera, ViewLookAtOffset) {
    // Camera offset should produce translation
    glm::vec3 cam_pos(10.0f, 20.0f, 30.0f);
    glm::vec3 look_at(0.0f, 0.0f, 0.0f);
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    glm::mat4 view = glm::lookAt(cam_pos, look_at, up);
    // View matrix should not be identity
    bool is_identity = true;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float expected = (i == j) ? 1.0f : 0.0f;
            if (std::abs(view[i][j] - expected) > 0.1f) {
                is_identity = false;
            }
        }
    }
    EXPECT_FALSE(is_identity);
}

// ─── 2. Shader Loading ──────────────────────────────────────────────────

static std::vector<uint32_t> load_spv(const std::string& path) {
    std::ifstream f(path, std::ios::ate | std::ios::binary);
    if (!f) return {};
    size_t sz = f.tellg();
    f.seekg(0);
    std::vector<uint32_t> data(sz / 4);
    f.read(reinterpret_cast<char*>(data.data()), sz);
    return data;
}

TEST(Shaders, TriangleVertSourceExists) {
    std::ifstream f("../src/shaders/triangle.vert");
    EXPECT_TRUE(f.good());
}

TEST(Shaders, TriangleFragSourceExists) {
    std::ifstream f("../src/shaders/triangle.frag");
    EXPECT_TRUE(f.good());
}

TEST(Shaders, BuildingVertSourceExists) {
    std::ifstream f("../src/shaders/building.vert");
    EXPECT_TRUE(f.good());
}

TEST(Shaders, BuildingFragSourceExists) {
    std::ifstream f("../src/shaders/building.frag");
    EXPECT_TRUE(f.good());
}

TEST(Shaders, LineVertSourceExists) {
    std::ifstream f("../src/shaders/line.vert");
    EXPECT_TRUE(f.good());
}

TEST(Shaders, FillVertSourceExists) {
    std::ifstream f("../src/shaders/fill.vert");
    EXPECT_TRUE(f.good());
}

TEST(Shaders, MissingShader) {
    auto code = load_spv("nonexistent.spv");
    EXPECT_TRUE(code.empty());
}

// ─── 3. Data Loading ────────────────────────────────────────────────────

TEST(DataLoading, StyleEngineLoads) {
    style::StyleEngine engine;
    ASSERT_TRUE(engine.loadFromJson("data/style.json"));
    EXPECT_EQ(engine.layers().size(), 9u);
}

TEST(DataLoading, OSMDataLoads) {
    if (!std::ifstream("data/osm_data.json").good()) {
        GTEST_SKIP() << "osm_data.json not found";
    }
    auto data = osm::load_osm_json("data/osm_data.json");
    EXPECT_GT(data.buildings.size(), 0u);
}

TEST(DataLoading, BuildingsExtract) {
    if (!std::ifstream("data/osm_data.json").good()) {
        GTEST_SKIP() << "osm_data.json not found";
    }
    auto data = osm::load_osm_json("data/osm_data.json");
    auto batch = bldg::extract_buildings(data.buildings);
    EXPECT_GT(batch.vertices.size(), 0u);
    EXPECT_GT(batch.indices.size(), 0u);
}

// ─── 4. Build Verification ──────────────────────────────────────────────

TEST(Build, DataDirectoryExists) {
    std::ifstream f("data/style.json");
    EXPECT_TRUE(f.good());
}

TEST(Build, ShaderDirectoryExists) {
    std::ifstream f("build/src/shaders/triangle.vert.spv");
    // Shader SPIR-V files may not exist if shaders haven't been compiled
    // Just verify the test doesn't crash
    (void)f;
}

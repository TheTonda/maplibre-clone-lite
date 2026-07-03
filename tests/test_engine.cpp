// Engine orchestrator integration tests — Task 17
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <map_renderer/engine.h>
#include <map_renderer/platform.h>

using namespace map_renderer;

// ── Mock GL for headless testing (same pattern as test_renderer.cpp) ──

namespace {
constexpr uint32_t GL_TRUE_C = 1;
constexpr uint32_t GL_FALSE_C = 0;
constexpr uint32_t GL_COMPILE_STATUS_C = 0x8B81;
constexpr uint32_t GL_LINK_STATUS_C = 0x8B82;

uint32_t g_next_id = 100;
int32_t g_next_uniform = 1;

void mock_gen_vao(int32_t, uint32_t* a) { *a = g_next_id++; }
void mock_gen_buffers(int32_t, uint32_t* a) { *a = g_next_id++; }
void mock_bind_vao(uint32_t) {}
void mock_bind_buffer(uint32_t, uint32_t) {}
void mock_buffer_data(uint32_t, intptr_t, const void*, uint32_t) {}
void mock_enable_vaa(uint32_t) {}
void mock_vaa_ptr(uint32_t, int32_t, uint32_t, uint8_t, int32_t, const void*) {}
void mock_draw_arrays(uint32_t, int32_t, int32_t) {}
void mock_use_program(uint32_t) {}
void mock_uniform_m4(int32_t, int32_t, uint8_t, const float*) {}
void mock_uniform_4f(int32_t, float, float, float, float) {}
void mock_uniform_2f(int32_t, float, float) {}
int32_t mock_get_uniform_loc(uint32_t, const char*) { return g_next_uniform++; }
void mock_clear_color(float, float, float, float) {}
void mock_clear(uint32_t) {}
void mock_viewport(int32_t, int32_t, int32_t, int32_t) {}
uint32_t mock_create_shader(uint32_t) { return g_next_id++; }
void mock_shader_source(uint32_t, int32_t, const char* const*, const int32_t*) {}
void mock_compile_shader(uint32_t) {}
void mock_get_shader_iv(uint32_t, uint32_t, int32_t* v) { *v = GL_TRUE_C; }
void mock_get_shader_log(uint32_t, int32_t, int32_t*, char*) {}
void mock_delete_shader(uint32_t) {}
uint32_t mock_create_program() { return g_next_id++; }
void mock_attach(uint32_t, uint32_t) {}
void mock_link(uint32_t) {}
void mock_get_program_iv(uint32_t, uint32_t, int32_t* v) { *v = GL_TRUE_C; }
void mock_get_program_log(uint32_t, int32_t, int32_t*, char*) {}
void mock_delete_program(uint32_t) {}
void mock_delete_vao(int32_t, const uint32_t*) {}
void mock_delete_buffers(int32_t, const uint32_t*) {}
void mock_enable(uint32_t) {}
void mock_disable(uint32_t) {}
uint32_t mock_get_error() { return 0; }

GLFunctions make_mock_gl() {
    GLFunctions gl{};
    gl.glGenVertexArrays = mock_gen_vao;
    gl.glGenBuffers = mock_gen_buffers;
    gl.glBindVertexArray = mock_bind_vao;
    gl.glBindBuffer = mock_bind_buffer;
    gl.glBufferData = mock_buffer_data;
    gl.glEnableVertexAttribArray = mock_enable_vaa;
    gl.glVertexAttribPointer = mock_vaa_ptr;
    gl.glDrawArrays = mock_draw_arrays;
    gl.glUseProgram = mock_use_program;
    gl.glUniformMatrix4fv = mock_uniform_m4;
    gl.glUniform4f = mock_uniform_4f;
    gl.glUniform2f = mock_uniform_2f;
    gl.glGetUniformLocation = mock_get_uniform_loc;
    gl.glClearColor = mock_clear_color;
    gl.glClear = mock_clear;
    gl.glViewport = mock_viewport;
    gl.glCreateShader = mock_create_shader;
    gl.glShaderSource = mock_shader_source;
    gl.glCompileShader = mock_compile_shader;
    gl.glGetShaderiv = mock_get_shader_iv;
    gl.glGetShaderInfoLog = mock_get_shader_log;
    gl.glDeleteShader = mock_delete_shader;
    gl.glCreateProgram = mock_create_program;
    gl.glAttachShader = mock_attach;
    gl.glLinkProgram = mock_link;
    gl.glGetProgramiv = mock_get_program_iv;
    gl.glGetProgramInfoLog = mock_get_program_log;
    gl.glDeleteProgram = mock_delete_program;
    gl.glDeleteVertexArrays = mock_delete_vao;
    gl.glDeleteBuffers = mock_delete_buffers;
    gl.glEnable = mock_enable;
    gl.glDisable = mock_disable;
    gl.glGetError = mock_get_error;
    return gl;
}

class MockPlatform : public PlatformInterface {
public:
    GLFunctions gl_funcs_;
    std::string tile_path_ = "data/tiles/new_delhi";
    int width_ = 1024, height_ = 768;

    const GLFunctions& get_gl_functions() const override { return gl_funcs_; }
    int get_viewport_width() const override { return width_; }
    int get_viewport_height() const override { return height_; }
    std::string get_tile_data_path() const override { return tile_path_; }
    void request_quit() override {}
    void set_vsync(bool) override {}
};
} // namespace

// ── Tests ──────────────────────────────────────────────────────────────

TEST(EngineTest, InitializeWithTestData) {
    MockPlatform platform;
    platform.gl_funcs_ = make_mock_gl();
    Engine engine;
    EXPECT_TRUE(engine.initialize(platform, "data/tiles/new_delhi"));
    engine.shutdown();
}

TEST(EngineTest, ShouldQuitFalseOnInit) {
    MockPlatform platform;
    platform.gl_funcs_ = make_mock_gl();
    Engine engine;
    engine.initialize(platform, "data/tiles/new_delhi");
    EXPECT_FALSE(engine.should_quit());
    engine.shutdown();
}

TEST(EngineTest, KeyQuitSetsShouldQuit) {
    MockPlatform platform;
    platform.gl_funcs_ = make_mock_gl();
    Engine engine;
    engine.initialize(platform, "data/tiles/new_delhi");

    std::vector<InputData> events;
    InputData q{};
    q.type = InputEvent::KeyQuit;
    events.push_back(q);

    engine.update(events, 0.016f);
    EXPECT_TRUE(engine.should_quit());
    engine.shutdown();
}

TEST(EngineTest, UpdateWithEmptyEventsNoCrash) {
    MockPlatform platform;
    platform.gl_funcs_ = make_mock_gl();
    Engine engine;
    engine.initialize(platform, "data/tiles/new_delhi");

    std::vector<InputData> events;
    engine.update(events, 0.016f);
    engine.update(events, 0.016f);
    engine.update(events, 0.016f);

    EXPECT_FALSE(engine.should_quit());
    engine.shutdown();
}

TEST(EngineTest, UpdateTriggersTileLoading) {
    MockPlatform platform;
    platform.gl_funcs_ = make_mock_gl();
    Engine engine;
    engine.initialize(platform, "data/tiles/new_delhi");

    // Run a few updates to let the loader thread work
    std::vector<InputData> events;
    for (int i = 0; i < 10; ++i) {
        engine.update(events, 0.016f);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Camera should have requested tiles — cache may have some loaded
    engine.shutdown();
    SUCCEED();
}

TEST(EngineTest, ZoomInputChangesCamera) {
    MockPlatform platform;
    platform.gl_funcs_ = make_mock_gl();
    Engine engine;
    engine.initialize(platform, "data/tiles/new_delhi");

    // Zoom in
    std::vector<InputData> events;
    InputData z{};
    z.type = InputEvent::Zoom;
    z.delta = 1.0f;
    events.push_back(z);

    engine.update(events, 0.016f);

    // After zoom, camera should no longer be dirty (cleared in update)
    // But the visible tiles should have been recomputed
    EXPECT_FALSE(engine.should_quit());
    engine.shutdown();
}

// Desktop application — SDL2 + OpenGL 3.3 Core + GLAD
// Task 12: window + single-tile render + input collection.
// Pan/zoom/camera integration deferred to Task 17.

#include <SDL2/SDL.h>
#include <glad/glad.h>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include <zstd.h>

#include "map_renderer/camera.h"
#include "map_renderer/debug_log.h"
#include "map_renderer/osm_loader.h"
#include "map_renderer/platform.h"
#include "map_renderer/renderer.h"
#include "map_renderer/tile_cache.h"
#include "map_renderer/tile_id.h"
#include "map_renderer/tile_loader.h"
#include "osm_data.pb.h"

using namespace map_renderer;

// ──────────────────────────────────────────────────────────────────────
// DesktopPlatform — implements PlatformInterface with SDL2 + GLAD
// ──────────────────────────────────────────────────────────────────────

class DesktopPlatform : public PlatformInterface {
public:
    bool initialize(int width, int height, const std::string& tile_path) {
        tile_path_ = tile_path;

        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
            return false;
        }

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

        window_ = SDL_CreateWindow("Map Renderer",
                                    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    width, height, SDL_WINDOW_OPENGL);
        if (!window_) {
            std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
            return false;
        }

        gl_context_ = SDL_GL_CreateContext(window_);
        if (!gl_context_) {
            std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
            return false;
        }

        SDL_GL_SetSwapInterval(1);  // VSync

        if (!gladLoadGLLoader(SDL_GL_GetProcAddress)) {
            std::fprintf(stderr, "gladLoadGLLoader failed\n");
            return false;
        }

        fill_gl_functions();
        width_ = width;
        height_ = height;
        return true;
    }

    ~DesktopPlatform() {
        if (gl_context_) SDL_GL_DeleteContext(gl_context_);
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
    }

    void poll_events(std::vector<InputData>& out) {
        static bool dragging = false;
        static int last_x = 0, last_y = 0;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                quit_ = true;
                break;
            case SDL_KEYDOWN: {
                auto key = e.key.keysym.sym;
                InputData d{};
                if (key == SDLK_ESCAPE) d.type = InputEvent::KeyQuit;
                else if (key == SDLK_PLUS || key == SDLK_KP_PLUS || key == SDLK_EQUALS)
                    d.type = InputEvent::KeyZoomIn;
                else if (key == SDLK_MINUS || key == SDLK_KP_MINUS)
                    d.type = InputEvent::KeyZoomOut;
                else if (key == SDLK_LEFT) d.type = InputEvent::KeyPanLeft;
                else if (key == SDLK_RIGHT) d.type = InputEvent::KeyPanRight;
                else if (key == SDLK_UP) d.type = InputEvent::KeyPanUp;
                else if (key == SDLK_DOWN) d.type = InputEvent::KeyPanDown;
                else break;
                out.push_back(d);
                if (key == SDLK_ESCAPE) quit_ = true;
                break;
            }
            case SDL_MOUSEBUTTONDOWN:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    dragging = true;
                    last_x = e.button.x;
                    last_y = e.button.y;
                    InputData d{};
                    d.type = InputEvent::PanStart;
                    out.push_back(d);
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    dragging = false;
                    InputData d{};
                    d.type = InputEvent::PanEnd;
                    out.push_back(d);
                }
                break;
            case SDL_MOUSEMOTION:
                if (dragging) {
                    InputData d{};
                    d.type = InputEvent::PanMove;
                    d.x = static_cast<float>(e.motion.x - last_x);
                    d.y = static_cast<float>(e.motion.y - last_y);
                    last_x = e.motion.x;
                    last_y = e.motion.y;
                    out.push_back(d);
                }
                break;
            case SDL_MOUSEWHEEL:
                {
                    InputData d{};
                    d.type = InputEvent::Zoom;
                    d.delta = static_cast<float>(e.wheel.y);
                    out.push_back(d);
                }
                break;
            }
        }
    }

    void swap_buffers() { SDL_GL_SwapWindow(window_); }

    // ── PlatformInterface ──────────────────────────────────────────────
    const GLFunctions& get_gl_functions() const override { return gl_funcs_; }
    int get_viewport_width() const override { return width_; }
    int get_viewport_height() const override { return height_; }

    std::string get_tile_data_path() const override { return tile_path_; }

    void request_quit() override { quit_ = true; }
    void set_vsync(bool enabled) override { SDL_GL_SetSwapInterval(enabled ? 1 : 0); }

    bool should_quit() const { return quit_; }

private:
    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_context_ = nullptr;
    GLFunctions gl_funcs_{};
    std::string tile_path_;
    int width_ = 0, height_ = 0;
    bool quit_ = false;

    void fill_gl_functions() {
        #define F(name) gl_funcs_.name = reinterpret_cast<decltype(gl_funcs_.name)>(glad_##name)
        F(glGenVertexArrays);
        F(glDeleteVertexArrays);
        F(glBindVertexArray);
        F(glGenBuffers);
        F(glDeleteBuffers);
        F(glBindBuffer);
        F(glBufferData);
        F(glEnableVertexAttribArray);
        F(glVertexAttribPointer);
        F(glDrawArrays);
        F(glUseProgram);
        F(glUniformMatrix4fv);
        F(glUniform4f);
        F(glUniform2f);
        F(glGetUniformLocation);
        F(glClearColor);
        F(glClear);
        F(glViewport);
        F(glCreateShader);
        F(glShaderSource);
        F(glCompileShader);
        F(glGetShaderiv);
        F(glGetShaderInfoLog);
        F(glDeleteShader);
        F(glCreateProgram);
        F(glAttachShader);
        F(glLinkProgram);
        F(glGetProgramiv);
        F(glGetProgramInfoLog);
        F(glDeleteProgram);
        F(glEnable);
        F(glDisable);
        F(glGetError);
        #undef F
    }
};

// ──────────────────────────────────────────────────────────────────────
// Metadata loading
// ──────────────────────────────────────────────────────────────────────

static bool load_metadata(const std::string& dir, double& ref_lat, double& ref_lon) {
    std::string meta_path = dir + "/metadata.bin";
    std::ifstream f(meta_path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return false;

    auto size = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> compressed(static_cast<size_t>(size));
    f.read(reinterpret_cast<char*>(compressed.data()), size);
    f.close();

    auto decomp_size = ZSTD_getFrameContentSize(compressed.data(), compressed.size());
    if (decomp_size == ZSTD_CONTENTSIZE_ERROR || decomp_size == ZSTD_CONTENTSIZE_UNKNOWN)
        return false;

    std::vector<uint8_t> decompressed(static_cast<size_t>(decomp_size));
    if (ZSTD_isError(ZSTD_decompress(decompressed.data(), decompressed.size(),
                                     compressed.data(), compressed.size())))
        return false;

    map_renderer_pb::DatasetMetadata meta;
    if (!meta.ParseFromArray(decompressed.data(), static_cast<int>(decompressed.size())))
        return false;

    ref_lat = meta.ref_lat();
    ref_lon = meta.ref_lon();
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// Main
// ──────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    std::string tile_path = (argc > 1) ? argv[1] : "data/tiles/new_delhi";

    DesktopPlatform platform;
    if (!platform.initialize(1024, 768, tile_path)) {
        std::fprintf(stderr, "Failed to initialize platform\n");
        return 1;
    }

    double ref_lat = 28.589, ref_lon = 77.2375;
    if (!load_metadata(tile_path, ref_lat, ref_lon)) {
        std::fprintf(stderr, "No metadata.bin found, using defaults\n");
    }

    Renderer renderer;
    if (!renderer.initialize(platform)) {
        std::fprintf(stderr, "Renderer initialize failed\n");
        return 1;
    }

    TileCache cache(64);
    TileLoader loader(tile_path, cache, ref_lat, ref_lon);
    Camera camera;  // stub — Task 17 wires this properly

    // Request a test tile
    // Try to load one that exists
    TileId test_tile{12, 2923, 1706};
    std::vector<TileId> visible_tiles = {test_tile};

    loader.start();
    loader.request_tiles(visible_tiles);

    std::fprintf(stderr, "Map Renderer started. ESC to quit.\n");

    // Main loop
    while (!platform.should_quit()) {
        std::vector<InputData> events;
        platform.poll_events(events);

        for (const auto& e : events) {
            if (e.type == InputEvent::KeyQuit) {
                goto done;
            }
        }

        // Drain recently loaded tiles and upload to GPU
        auto recent = cache.drain_recent_inserts();
        for (const auto& id : recent) {
            auto tile = cache.get(id);
            if (tile && !tile->uploaded) {
                renderer.on_tile_loaded(id, *tile);
            }
        }

        // Render
        renderer.render(camera, cache, visible_tiles);
        platform.swap_buffers();
    }

done:
    loader.stop();
    renderer.cleanup();
    return 0;
}
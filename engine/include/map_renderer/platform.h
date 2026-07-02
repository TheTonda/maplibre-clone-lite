#pragma once

#include <cstdint>
#include <string>

namespace map_renderer {

// GL function pointers are app-specific. The app loads them (via GLAD)
// and passes a struct of needed function pointers to the engine.
//
// IMPORTANT: This struct uses standard C++ types (uint32_t, int32_t, etc.)
// instead of GL types (GLuint, GLsizei, etc.) so that platform.h does NOT
// need to include any GL headers. This preserves the "no GL headers in
// engine public headers" rule (FR-5.3, NFR-2.1) and allows headless tests
// to compile without a GL SDK installed. The app casts GLAD-loaded
// function pointers to these signatures when filling the struct (e.g.
// reinterpret_cast<void(*)(int32_t, uint32_t*)>(glGenVertexArrays)) — the
// types have identical binary representation on all target platforms.
struct GLFunctions {
    // Core functions needed by the engine
    void (*glGenVertexArrays)(int32_t, uint32_t*);
    void (*glDeleteVertexArrays)(int32_t, const uint32_t*);
    void (*glBindVertexArray)(uint32_t);
    void (*glGenBuffers)(int32_t, uint32_t*);
    void (*glDeleteBuffers)(int32_t, const uint32_t*);
    void (*glBindBuffer)(uint32_t, uint32_t);
    void (*glBufferData)(uint32_t, intptr_t, const void*, uint32_t);
    void (*glEnableVertexAttribArray)(uint32_t);
    void (*glVertexAttribPointer)(uint32_t, int32_t, uint32_t, uint8_t, int32_t, const void*);
    void (*glDrawArrays)(uint32_t, int32_t, int32_t);
    void (*glUseProgram)(uint32_t);
    void (*glUniformMatrix4fv)(int32_t, int32_t, uint8_t, const float*);
    void (*glUniform4f)(int32_t, float, float, float, float);
    void (*glUniform2f)(int32_t, float, float);
    void (*glGetUniformLocation)(uint32_t, const char*);
    void (*glClearColor)(float, float, float, float);
    void (*glClear)(uint32_t);
    void (*glViewport)(int32_t, int32_t, int32_t, int32_t);
    uint32_t (*glCreateShader)(uint32_t);
    void (*glShaderSource)(uint32_t, int32_t, const char* const*, const int32_t*);
    void (*glCompileShader)(uint32_t);
    void (*glGetShaderiv)(uint32_t, uint32_t, int32_t*);
    void (*glGetShaderInfoLog)(uint32_t, int32_t, int32_t*, char*);
    void (*glDeleteShader)(uint32_t);
    uint32_t (*glCreateProgram)(void);
    void (*glAttachShader)(uint32_t, uint32_t);
    void (*glLinkProgram)(uint32_t);
    void (*glGetProgramiv)(uint32_t, uint32_t, int32_t*);
    void (*glGetProgramInfoLog)(uint32_t, int32_t, int32_t*, char*);
    void (*glDeleteProgram)(uint32_t);
    void (*glEnable)(uint32_t);
    void (*glDisable)(uint32_t);
    uint32_t (*glGetError)(void);
};

// Input events pushed by the app into the engine
enum class InputEvent {
    PanStart,
    PanMove,
    PanEnd,
    Zoom,
    KeyQuit,
    KeyZoomIn,
    KeyZoomOut,
    KeyPanLeft,
    KeyPanRight,
    KeyPanUp,
    KeyPanDown,
};

struct InputData {
    InputEvent type;
    float x = 0.0f;   // for pan move (screen delta), or zoom center
    float y = 0.0f;
    float delta = 0.0f;  // for zoom
};

class PlatformInterface {
public:
    virtual ~PlatformInterface() = default;

    // GL functions (loaded by app via GLAD, passed to engine)
    virtual const GLFunctions& get_gl_functions() const = 0;

    // Viewport
    virtual int get_viewport_width() const = 0;
    virtual int get_viewport_height() const = 0;

    // Filesystem
    virtual std::string get_tile_data_path() const = 0;

    // Called by engine when it wants to quit
    virtual void request_quit() = 0;

    // VSync control
    virtual void set_vsync(bool enabled) = 0;
};

} // namespace map_renderer

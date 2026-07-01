#pragma once

/// @file window.h
/// @brief SDL2 window abstraction with Vulkan surface and input polling.

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <string>

#include "input_state.h"

class Window {
public:
    /// Construct and show an SDL window.
    Window(const std::string& title, int width, int height);

    /// Destructor – cleans up SDL resources.
    ~Window();

    // Non-copyable
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    /// Poll all pending SDL events and write the result into @p state.
    void poll_events(InputState& state);

    /// @return true when the user has requested the window to close.
    bool should_close() const { return should_close_; }

    /// @return true if the window was resized since the last poll.
    bool was_resized() const { return resized_; }

    /// Reset the resize flag (call after handling the resize).
    void reset_resized() { resized_ = false; }

    /// Set a new window title (e.g. for FPS display).
    void set_title(const std::string& title);

    /// Programmatically request a close.
    void close() { should_close_ = true; }

    // ---- Accessors ----

    SDL_Window*  get_sdl_window() const { return window_; }
    int get_width()  const { return width_; }
    int get_height() const { return height_; }

private:
    SDL_Window*    window_   = nullptr;
    int            width_    = 0;
    int            height_   = 0;
    bool           resized_  = false;
    bool           should_close_ = false;

    // Cursor tracking for mouse deltas
    int last_mouse_x_ = 0;
    int last_mouse_y_ = 0;
    bool mouse_initialised_ = false;

    // Frame timing
    uint64_t last_tick_ = 0;
};

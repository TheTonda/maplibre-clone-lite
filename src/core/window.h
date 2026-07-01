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
    /// Construct and show an SDL window with a Vulkan-compatible surface.
    Window(const std::string& title, int width, int height);

    /// Destructor – cleans up SDL and the Vulkan surface.
    ~Window();

    // Non-copyable
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    /// Poll all pending SDL events and write the result into @p state.
    void poll_events(InputState& state);

    /// @return true when the user has requested the window to close.
    bool should_close() const { return should_close_; }

    /// Programmatically request a close.
    void close() { should_close_ = true; }

    // ---- Accessors ----

    SDL_Window*  get_sdl_window() const { return window_; }
    VkSurfaceKHR get_surface()    const { return surface_; }
    int get_width()  const { return width_; }
    int get_height() const { return height_; }

private:
    SDL_Window*    window_   = nullptr;
    VkSurfaceKHR   surface_  = VK_NULL_HANDLE;
    int            width_    = 0;
    int            height_   = 0;
    bool           should_close_ = false;

    // Cursor tracking for mouse deltas
    int last_mouse_x_ = 0;
    int last_mouse_y_ = 0;
    bool mouse_initialised_ = false;

    // Frame timing
    uint64_t last_tick_ = 0;
};

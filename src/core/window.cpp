/// @file window.cpp
/// @brief SDL2 window implementation with Vulkan surface.

#include "core/window.h"

#include <cstdio>

// -----------------------------------------------------------------------
// Construction / Destruction
// -----------------------------------------------------------------------

Window::Window(const std::string& title, int width, int height)
    : width_(width)
    , height_(height)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "[ERROR] SDL_Init failed: %s\n", SDL_GetError());
        std::abort();
    }

    window_ = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width_, height_,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
    );
    if (!window_) {
        std::fprintf(stderr, "[ERROR] SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        std::abort();
    }

    // Create Vulkan surface from SDL window
    if (!SDL_Vulkan_CreateSurface(window_, VK_NULL_HANDLE, &surface_)) {
        std::fprintf(stderr, "[ERROR] SDL_Vulkan_CreateSurface failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window_);
        SDL_Quit();
        std::abort();
    }

    last_tick_ = SDL_GetTicks64();
    mouse_initialised_ = false;

    std::fprintf(stdout, "[INFO]  Window created: %dx%d\n", width_, height_);
}

Window::~Window() {
    if (surface_ != VK_NULL_HANDLE) {
        // Surface must be destroyed *before* the Vulkan instance, but since
        // we don't own the instance here the caller must clean up the surface.
        // We just destroy the window / quit SDL.
    }
    if (window_) {
        SDL_DestroyWindow(window_);
    }
    SDL_Quit();
    std::fprintf(stdout, "[INFO]  Window destroyed.\n");
}

// -----------------------------------------------------------------------
// Event Polling
// -----------------------------------------------------------------------

void Window::poll_events(InputState& state) {
    // --- Frame timing ---
    uint64_t now  = SDL_GetTicks64();
    state.dt      = static_cast<float>(now - last_tick_) / 1000.0f;
    last_tick_    = now;

    // --- Reset single-frame flags ---
    state.reset_frame_state();

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        // -- Window close --
        case SDL_QUIT:
            should_close_ = true;
            break;

        // -- Keyboard --
        case SDL_KEYDOWN:
        case SDL_KEYUP: {
            bool pressed = (ev.type == SDL_KEYDOWN);
            switch (ev.key.keysym.sym) {
            case SDLK_ESCAPE:  should_close_ = true;         break;
            case SDLK_UP:    case SDLK_w: state.up          = pressed; break;
            case SDLK_DOWN:  case SDLK_s: state.down        = pressed; break;
            case SDLK_LEFT:  case SDLK_a: state.rotate_left = pressed;
                                        state.left         = pressed; break;
            case SDLK_RIGHT: case SDLK_d: state.rotate_right= pressed;
                                        state.right        = pressed; break;
            case SDLK_EQUALS: case SDLK_PLUS:
                                state.zoom_in  = pressed; break;
            case SDLK_MINUS:  state.zoom_out = pressed; break;
            case SDLK_q:      state.tilt_up   = pressed; break;
            case SDLK_e:      state.tilt_down = pressed; break;
            case SDLK_F1:     if (pressed) state.switch_2d = true; break;
            case SDLK_F2:     if (pressed) state.switch_3d = true; break;
            default: break;
            }
            break;
        }

        // -- Mouse button --
        case SDL_MOUSEBUTTONDOWN:
            if (ev.button.button == SDL_BUTTON_LEFT) {
                state.left_mouse_down = true;
                state.mouse_pressed   = true;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (ev.button.button == SDL_BUTTON_LEFT) {
                state.left_mouse_down = false;
                state.mouse_released  = true;
            }
            break;

        // -- Mouse motion --
        case SDL_MOUSEMOTION: {
            state.mouse_x = ev.motion.x;
            state.mouse_y = ev.motion.y;
            if (mouse_initialised_) {
                state.delta_x = ev.motion.x - last_mouse_x_;
                state.delta_y = ev.motion.y - last_mouse_y_;
            } else {
                mouse_initialised_ = true;
            }
            last_mouse_x_ = ev.motion.x;
            last_mouse_y_ = ev.motion.y;
            break;
        }

        // -- Mouse wheel --
        case SDL_MOUSEWHEEL:
            state.scroll_delta = static_cast<float>(ev.wheel.y);
            break;

        default:
            break;
        }
    }
}

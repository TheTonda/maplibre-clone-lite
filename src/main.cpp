/// @file main.cpp
/// @brief Interactive 3D Map Renderer – Entry Point
///
/// Creates the SDL window, initialises Vulkan, and runs the main loop
/// until the user closes the window.

#include "core/debug_log.h"
#include "core/window.h"
#include "core/vulkan_context.h"
#include "core/input_state.h"

int main() {
    DEBUG_LOG("map-renderer starting...");

    constexpr int WIDTH  = 1280;
    constexpr int HEIGHT = 720;

    Window window("Map Renderer", WIDTH, HEIGHT);

    VulkanContext vk_ctx;
    vk_ctx.initialize(window);

    InputState input_state;

    // Main loop
    while (!window.should_close()) {
        window.poll_events(input_state);

        if (input_state.switch_2d) {
            DEBUG_LOG("Switch to 2D mode requested");
        }
        if (input_state.switch_3d) {
            DEBUG_LOG("Switch to 3D mode requested");
        }
        if (input_state.zoom_in || input_state.zoom_out) {
            DEBUG_LOG("Zoom: dt=%.3f", input_state.dt);
        }

        // TODO: Render frame (Task 4+)
    }

    vk_ctx.cleanup();
    DEBUG_LOG("map-renderer exiting.");
    return 0;
}

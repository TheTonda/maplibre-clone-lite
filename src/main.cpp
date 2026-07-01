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

        // Acquire the next swapchain image
        uint32_t image_index = vk_ctx.acquire_next_image();
        if (image_index == ~0u) {
            // Swapchain is out of date — skip this frame
            input_state.reset_frame_state();
            continue;
        }

        // Record + submit draw commands, then present
        // TODO: in later tasks the Renderer class will inject draw calls
        //       between cmdBeginRenderPass and cmdEndRenderPass.
        vk_ctx.submit_frame(image_index);

        input_state.reset_frame_state();
    }

    vk_ctx.cleanup();
    DEBUG_LOG("map-renderer exiting.");
    return 0;
}

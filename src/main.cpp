/// @file main.cpp
/// @brief Interactive 3D Map Renderer – Entry Point

#include "core/debug_log.h"
#include "core/window.h"
#include "core/vulkan_context.h"
#include "core/input_state.h"
#include "core/camera.h"
#include "render/renderer.h"
#include "data/geometry_builder.h"

#include <cstdio>
#include <string>

/// C-style callback for VulkanContext::submit_frame.
static void record_draw_callback(VkCommandBuffer cmd, void* user_data) {
    Renderer* renderer = static_cast<Renderer*>(user_data);
    renderer->record_draw_commands(cmd, 0);
}

int main() {
    DEBUG_LOG("map-renderer starting...");

    constexpr int WIDTH  = 1280;
    constexpr int HEIGHT = 720;

    Window window("Map Renderer", WIDTH, HEIGHT);
    VulkanContext vk_ctx;
    vk_ctx.initialize(window);

    InputState input_state;
    Camera camera;
    Renderer renderer;

    // Initialise renderer (must happen after VulkanContext is ready)
    renderer.initialize(vk_ctx);

    // Create a default geometry set so we have something to render
    GeometryData geo;
    geo.ground = GeometryBuilder::build_ground(5000.0f);
    renderer.set_geometry(geo);

    // Set camera to reasonable default
    camera.set_position(0.0f, 0.0f);

    float aspect = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);

    // ---- FPS tracking ----
    uint32_t frame_count  = 0;
    uint32_t fps_tick     = SDL_GetTicks64();
    constexpr uint32_t FPS_INTERVAL = 1000;  // ms

    // Main loop
    while (!window.should_close()) {
        window.poll_events(input_state);

        // Handle window resize
        if (window.was_resized()) {
            vk_ctx.recreate_swapchain(window);
            window.reset_resized();

            // Update descriptor sets with new buffers? No — the camera UBO
            // and geometry buffers stay the same.  Only swapchain-dependent
            // resources (framebuffers, render pass) were recreated.
            aspect = static_cast<float>(window.get_width()) /
                     static_cast<float>(window.get_height());
        }

        camera.update_from_input(input_state);

        // Acquire the next swapchain image
        uint32_t image_index = vk_ctx.acquire_next_image();
        if (image_index == ~0u) {
            // Swapchain out of date — recreate and skip this frame
            vk_ctx.recreate_swapchain(window);
            input_state.reset_frame_state();
            continue;
        }

        // Update camera UBO
        renderer.update_camera(camera, aspect);

        // Record + submit draw commands, then present
        vk_ctx.submit_frame(image_index, record_draw_callback, &renderer);

        input_state.reset_frame_state();

        // ---- FPS counter ----
        frame_count++;
        uint32_t now = SDL_GetTicks64();
        if (now - fps_tick >= FPS_INTERVAL) {
            float fps = static_cast<float>(frame_count) * 1000.0f /
                        static_cast<float>(now - fps_tick);
            char title[128];
            std::snprintf(title, sizeof(title),
                          "Map Renderer — %.0f FPS", fps);
            window.set_title(title);
            frame_count = 0;
            fps_tick    = now;
        }
    }

    vk_ctx.cleanup();
    renderer.cleanup();
    DEBUG_LOG("map-renderer exiting.");
    return 0;
}

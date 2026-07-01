#include <gtest/gtest.h>
#include <cstdlib>
#include "core/debug_log.h"
#include "core/window.h"
#include "core/vulkan_context.h"
#include "core/camera.h"
#include "render/renderer.h"
#include "data/geometry_builder.h"

/// Environment check — skip if DISPLAY is not set.
static bool has_display() {
    const char* display = std::getenv("DISPLAY");
    const char* wayland = std::getenv("WAYLAND_DISPLAY");
    return (display && display[0] != '\0') ||
           (wayland && wayland[0] != '\0');
}

/// Integration test: creates a Vulkan context + renderer, checks geometry
/// upload, then cleans up.  Skipped if no display is available.
TEST(VulkanIntegration, FullLifecycle) {
    if (!has_display()) {
        GTEST_SKIP() << "No display available — skipping Vulkan integration test";
    }

    DEBUG_LOG("Integration test: creating window...");

    Window window("Integration Test", 640, 480);
    VulkanContext ctx;

    // Initialise Vulkan
    ctx.initialize(window);

    ASSERT_NE(ctx.get_device(), VK_NULL_HANDLE);
    ASSERT_NE(ctx.get_render_pass(), VK_NULL_HANDLE);

    // Create the renderer and upload geometry.
    Renderer renderer;
    renderer.initialize(ctx);

    Camera camera;
    camera.set_frame_bounds(0.0, 0.0, 500.0, 500.0);

    GeometryData geo;
    geo.ground = GeometryBuilder::build_ground(5000.0f);
    renderer.set_geometry(geo);

    // Run a single frame
    InputState input;
    window.poll_events(input);
    camera.update_from_input(input);
    renderer.update_camera(camera, 640.0f / 480.0f);

    uint32_t idx = ctx.acquire_next_image();
    if (idx == ~0u) {
        renderer.cleanup();
        ctx.cleanup();
        GTEST_SKIP() << "Swapchain out of date — skipping draw test";
    }
    ctx.submit_frame(idx, nullptr, nullptr);

    // Clean up while device is still alive
    renderer.cleanup();
    ctx.cleanup();

    SUCCEED();
}

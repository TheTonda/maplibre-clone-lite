/// @file main.cpp
/// @brief Interactive 3D Map Renderer – Entry Point

#include "core/debug_log.h"
#include "core/window.h"
#include "core/vulkan_context.h"
#include "core/input_state.h"
#include "core/camera.h"
#include "render/renderer.h"
#include "data/geometry_builder.h"
#include "data/osm_loader.h"

#include <cstdio>
#include <cstdlib>
#include <string>

/// C-style callback for VulkanContext::submit_frame.
static void record_draw_callback(VkCommandBuffer cmd, void* user_data) {
    auto* renderer = static_cast<Renderer*>(user_data);
    renderer->record_draw_commands(cmd, 0);
}

int main(int argc, char** argv) {
    // Disable stdout buffering so log output appears immediately
    setvbuf(stdout, nullptr, _IONBF, 0);

    DEBUG_LOG("map-renderer starting...");

    // ---- CLI: optional data file ----
    std::string data_path = "data/test_scene.osm_data";
    if (argc >= 2) {
        data_path = argv[1];
    }

    // ---- Load OSM data (try multiple paths) ----
    osm::OSMData osm_data;
    float bounds_margin = 250.0f;   // extra space around data
    float data_min_x = -bounds_margin, data_max_x = bounds_margin;
    float data_min_z = -bounds_margin, data_max_z = bounds_margin;

    // Try the data file at several relative locations
    osm_data = OSMLoader::load_from_file(data_path);
    if (!osm_data.has_data()) {
        std::string alt = "../" + data_path;
        osm_data = OSMLoader::load_from_file(alt);
    }

    if (!osm_data.has_data()) {
        std::fprintf(stdout, "[main] No OSM data loaded — ground plane only.\n");
    } else {
        data_min_x = osm_data.min_x - bounds_margin;
        data_max_x = osm_data.max_x + bounds_margin;
        data_min_z = osm_data.min_z - bounds_margin;
        data_max_z = osm_data.max_z + bounds_margin;
        std::fprintf(stdout, "[main] Loaded %zu buildings, %zu roads, %zu parks, "
                     "%zu water, %zu landuse.\n",
                     osm_data.buildings.size(), osm_data.roads.size(),
                     osm_data.parks.size(), osm_data.water.size(),
                     osm_data.landuse.size());
        std::fprintf(stdout, "[main] Bounds: X[%.0f, %.0f] Z[%.0f, %.0f]\n",
                     osm_data.min_x, osm_data.max_x,
                     osm_data.min_z, osm_data.max_z);
    }

    // ---- Window + Vulkan ----
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

    // Build geometry from OSM data — size ground to data extent
    float ground_half = std::max({
        std::abs(data_min_x), std::abs(data_max_x),
        std::abs(data_min_z), std::abs(data_max_z)
    }) * 1.2f;
    if (ground_half < 500.0f) ground_half = 500.0f;

    GeometryData geo = GeometryBuilder::build_all(osm_data, ground_half);
    renderer.set_geometry(geo);

    // Set camera bounds to cover the data
    camera.set_frame_bounds(data_min_x, data_min_z, data_max_x, data_max_z);

    float aspect = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);

    // ---- FPS tracking ----
    uint32_t frame_count  = 0;
    uint32_t fps_tick     = SDL_GetTicks64();
    constexpr uint32_t FPS_INTERVAL = 1000;

    // ---- Main loop ----
    while (!window.should_close()) {
        window.poll_events(input_state);

        if (window.was_resized()) {
            vk_ctx.recreate_swapchain(window);
            window.reset_resized();
            aspect = static_cast<float>(window.get_width()) /
                     static_cast<float>(window.get_height());
        }

        camera.update_from_input(input_state);

        uint32_t image_index = vk_ctx.acquire_next_image();
        if (image_index == ~0u) {
            vk_ctx.recreate_swapchain(window);
            input_state.reset_frame_state();
            continue;
        }

        renderer.update_camera(camera, aspect);
        vk_ctx.submit_frame(image_index, record_draw_callback, &renderer);
        input_state.reset_frame_state();

        // FPS counter
        frame_count++;
        uint32_t now = SDL_GetTicks64();
        if (now - fps_tick >= FPS_INTERVAL) {
            float fps = static_cast<float>(frame_count) * 1000.0f /
                        static_cast<float>(now - fps_tick);
            char title[128];
            std::snprintf(title, sizeof(title), "Map Renderer — %.0f FPS", fps);
            window.set_title(title);
            frame_count = 0;
            fps_tick    = now;
        }
    }

    renderer.cleanup();
    vk_ctx.cleanup();
    DEBUG_LOG("map-renderer exiting.");
    return 0;
}

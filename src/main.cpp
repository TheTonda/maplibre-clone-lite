// map-renderer-v2 — Vulkan colored triangle renderer
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <fstream>
#include <algorithm>
#include <optional>
#include <cmath>

#include "style_engine.h"
#include "mvt_parser.h"
#include "render_data.h"
#include "osm_loader.h"
#include "building_data.h"
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

// GLM for camera matrix math
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

constexpr int WIDTH = 1024;
constexpr int HEIGHT = 768;

// --- Debug messenger ---
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*) {
    fprintf(stderr, "Vulkan: %s\n", data->pMessage);
    return VK_FALSE;
}

// --- SPIR-V loader ---
static std::vector<uint32_t> load_spv(const char* path) {
    std::ifstream f(path, std::ios::ate | std::ios::binary);
    if (!f) {
        fprintf(stderr, "Failed to open SPIR-V: %s\n", path);
        return {};
    }
    size_t sz = f.tellg();
    f.seekg(0);
    std::vector<uint32_t> data(sz / 4);
    f.read(reinterpret_cast<char*>(data.data()), sz);
    return data;
}

// --- Queue family picker ---
struct QueueFamilies {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;

    bool complete() const { return graphics.has_value() && present.has_value(); }

    static QueueFamilies find(VkPhysicalDevice dev, VkSurfaceKHR surface) {
        QueueFamilies qf;
        uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
        std::vector<VkQueueFamilyProperties> props(count);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, props.data());

        for (uint32_t i = 0; i < count; i++) {
            if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                qf.graphics = i;

            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &present);
            if (present)
                qf.present = i;

            if (qf.complete()) break;
        }
        return qf;
    }
};

// --- Swapchain support ---
struct SwapchainSupport {
    VkSurfaceCapabilitiesKHR caps;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;

    static SwapchainSupport query(VkPhysicalDevice dev, VkSurfaceKHR surface) {
        SwapchainSupport s;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &s.caps);

        uint32_t count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &count, nullptr);
        if (count) {
            s.formats.resize(count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &count, s.formats.data());
        }

        vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &count, nullptr);
        if (count) {
            s.present_modes.resize(count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &count, s.present_modes.data());
        }
        return s;
    }

    VkSurfaceFormatKHR choose_format() const {
        for (auto& f : formats)
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                return f;
        return formats[0];
    }

    VkPresentModeKHR choose_present_mode() const {
        for (auto& m : present_modes)
            if (m == VK_PRESENT_MODE_MAILBOX_KHR)
                return m;
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D choose_extent() const {
        if (caps.currentExtent.width != UINT32_MAX)
            return caps.currentExtent;
        return {
            std::clamp((uint32_t)WIDTH, caps.minImageExtent.width, caps.maxImageExtent.width),
            std::clamp((uint32_t)HEIGHT, caps.minImageExtent.height, caps.maxImageExtent.height)
        };
    }
};

// --- Shader module helper ---
static VkShaderModule create_shader_module(VkDevice dev, const std::vector<uint32_t>& code) {
    VkShaderModuleCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.size() * sizeof(uint32_t),
        .pCode = code.data(),
    };
    VkShaderModule mod;
    vkCreateShaderModule(dev, &info, nullptr, &mod);
    return mod;
}

// --- Helper: find host-visible memory type index ---
static uint32_t find_host_mem_type(VkPhysicalDevice physical, uint32_t type_bits) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical, &mem_props);
    for (uint32_t j = 0; j < mem_props.memoryTypeCount; ++j) {
        if ((type_bits & (1 << j)) &&
            (mem_props.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            return j;
        }
    }
    return UINT32_MAX;
}

// --- Clamp helper (C++17 compatible) ---
template<typename T>
static T clamp_val(T v, T lo, T hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

int main() {
    // --- SDL Window ---
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Map Renderer v2 — Camera",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIDTH, HEIGHT,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        fprintf(stderr, "No display available — exiting gracefully.\n");
        SDL_Quit();
        return 0;
    }

    // --- Vulkan Instance ---
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Map Renderer v2",
        .applicationVersion = VK_MAKE_VERSION(0, 2, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };

    unsigned ext_count;
    SDL_Vulkan_GetInstanceExtensions(window, &ext_count, nullptr);
    std::vector<const char*> extensions(ext_count);
    SDL_Vulkan_GetInstanceExtensions(window, &ext_count, extensions.data());
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkDebugUtilsMessengerCreateInfoEXT debug_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                      | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        .pfnUserCallback = debug_callback,
    };

    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = &debug_info,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = (uint32_t)extensions.size(),
        .ppEnabledExtensionNames = extensions.data(),
    };

    VkInstance instance;
    if (vkCreateInstance(&instance_info, nullptr, &instance) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan instance\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("Vulkan instance created.\n");

    // --- Surface ---
    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
        fprintf(stderr, "Failed to create surface: %s\n", SDL_GetError());
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // --- Physical Device ---
    uint32_t device_count;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
    VkPhysicalDevice physical = devices[0];

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physical, &props);
    printf("GPU: %s\n", props.deviceName);

    // --- Style Engine ---
    style::StyleEngine styleEngine;
    if (!styleEngine.loadFromJson("data/style.json")) {
        fprintf(stderr, "Warning: could not load style, using defaults\n");
    }
    styleEngine.print();

    // Match color for the demo triangle (layer "triangle", type fill/polygon)
    style::StyleRule rule = styleEngine.matchRule("triangle", std::string("fill"));
    printf("Triangle color from style: #%02x%02x%02x\n",
           int(rule.fill_color[0] * 255),
           int(rule.fill_color[1] * 255),
           int(rule.fill_color[2] * 255));

    // --- Camera variables ---
    float camera_x = 0.0f;
    float camera_y = 0.0f;
    float zoom_level = 14.0f;
    float tilt_angle = 45.0f;  // Default tilt to see 3D buildings
    int mode = 2;

    // --- Load OSM data ---
    osm::OSMData osmData = osm::load_osm_json("data/osm_data.json");

    // --- Compute data bounds and center camera ---
    float min_x = 1e9f, min_y = 1e9f, max_x = -1e9f, max_y = -1e9f;
    
    auto update_bounds = [&](float x, float y) {
        if (x < min_x) min_x = x;
        if (y < min_y) min_y = y;
        if (x > max_x) max_x = x;
        if (y > max_y) max_y = y;
    };
    
    for (const auto& b : osmData.buildings) {
        for (const auto& p : b.footprint) {
            update_bounds(p.x, p.y);
        }
    }
    for (const auto& r : osmData.roads) {
        for (const auto& p : r.line) {
            update_bounds(p.x, p.y);
        }
    }
    for (const auto& p : osmData.parks) {
        for (const auto& pt : p.polygon) {
            update_bounds(pt.x, pt.y);
        }
    }
    for (const auto& w : osmData.water_polygons) {
        for (const auto& pt : w.polygon) {
            update_bounds(pt.x, pt.y);
        }
    }
    for (const auto& l : osmData.landuse) {
        for (const auto& pt : l.polygon) {
            update_bounds(pt.x, pt.y);
        }
    }
    
    float center_x = (min_x + max_x) * 0.5f;
    float center_y = (min_y + max_y) * 0.5f;
    float range_x = (max_x - min_x) * 0.5f;
    float range_y = (max_y - min_y) * 0.5f;
    float max_range = std::max(range_x, range_y);
    
    printf("Data bounds: (%.0f,%.0f) to (%.0f,%.0f)\n", min_x, min_y, max_x, max_y);
    printf("Data center: (%.0f,%.0f), range: %.0f\n", center_x, center_y, max_range * 2);

    // Camera starts at origin (buildings are normalized to origin)
    camera_x = 0.0f;
    camera_y = 0.0f;

    // Calculate zoom for 2D mode (based on original data bounds)
    float padding = 1.5f;
    zoom_level = 2.0f / (max_range * padding);
    if (zoom_level < 0.001f) zoom_level = 0.001f;

    printf("Initial zoom: %.6f\n", zoom_level);

    // --- Extract building geometry ---
    bldg::BuildingBatch buildingBatch = bldg::extract_buildings(osmData.buildings);

    // --- Compute maximum building height for camera positioning ---
    float max_building_height = 0.0f;
    for (const auto& v : buildingBatch.vertices) {
        if (v.y > max_building_height) {
            max_building_height = v.y;
        }
    }
    printf("Max building height: %.1f\n", max_building_height);

    // --- Compute building bounds (already normalized to origin) ---
    float bldg_min_x = 1e9f, bldg_min_y = 1e9f, bldg_min_z = 1e9f;
    float bldg_max_x = -1e9f, bldg_max_y = -1e9f, bldg_max_z = -1e9f;
    for (const auto& v : buildingBatch.vertices) {
        if (v.x < bldg_min_x) bldg_min_x = v.x;
        if (v.y < bldg_min_y) bldg_min_y = v.y;
        if (v.z < bldg_min_z) bldg_min_z = v.z;
        if (v.x > bldg_max_x) bldg_max_x = v.x;
        if (v.y > bldg_max_y) bldg_max_y = v.y;
        if (v.z > bldg_max_z) bldg_max_z = v.z;
    }
    printf("Building bounds: X(%.0f to %.0f) Y(0 to %.0f) Z(%.0f to %.0f)\n",
           bldg_min_x, bldg_max_x, bldg_max_y, bldg_min_z, bldg_max_z);

    // --- Extract 2D fills for parks, water, landuse ---
    style::StyleRule parkRule = styleEngine.matchRule("parks", std::string("fill"));
    style::StyleRule waterRule = styleEngine.matchRule("water", std::string("fill"));
    style::StyleRule landuseRule = styleEngine.matchRule("landuse", std::string("fill"));

    auto fills2d = bldg::extract_fills_2d(
        osmData.parks, osmData.water_polygons, osmData.landuse,
        glm::vec3(parkRule.fill_color[0], parkRule.fill_color[1], parkRule.fill_color[2]),
        glm::vec3(waterRule.fill_color[0], waterRule.fill_color[1], waterRule.fill_color[2]),
        glm::vec3(landuseRule.fill_color[0], landuseRule.fill_color[1], landuseRule.fill_color[2])
    );

    // --- Load MVT tile (optional, for backward compatibility) ---
    std::ifstream mvt_file("data/test_roads.mvt", std::ios::binary | std::ios::ate);
    render::LineBatch line_batch;
    render::PolyBatch poly_batch;
    std::vector<char> mvt_buf;
    if (mvt_file) {
        std::streamsize mvt_size = mvt_file.tellg();
        mvt_file.seekg(0, std::ios::beg);
        mvt_buf.resize(static_cast<size_t>(mvt_size));
        mvt_file.read(mvt_buf.data(), mvt_size);
        mvt_file.close();

        // Parse once for lines
        {
            google::protobuf::io::ArrayInputStream array_input(mvt_buf.data(),
                                                                static_cast<int>(mvt_size));
            google::protobuf::io::CodedInputStream coded_input(&array_input);
            mvt::Tile tile = mvt::parse_tile(&coded_input);

            line_batch = render::extract_lines(tile, "streets");
            printf("Extracted line features: %zu vertices, %zu indices\n",
                   line_batch.vertices.size(), line_batch.indices.size());
        }

        // Parse again for polygons (simpler than keeping tile alive)
        {
            google::protobuf::io::ArrayInputStream array_input(mvt_buf.data(),
                                                                static_cast<int>(mvt_size));
            google::protobuf::io::CodedInputStream coded_input(&array_input);
            mvt::Tile tile2 = mvt::parse_tile(&coded_input);

            poly_batch = render::extract_polygons(tile2, "");
            printf("Extracted polygon features: %zu vertices, %zu indices\n",
                   poly_batch.vertices.size(), poly_batch.indices.size());
        }
    } else {
        printf("No MVT tile found, using OSM data only\n");
    }

    // --- Queue Families ---
    QueueFamilies qf = QueueFamilies::find(physical, surface);
    if (!qf.complete()) {
        fprintf(stderr, "No suitable queue family\n");
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // --- Logical Device ---
    float queue_priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queue_infos;
    if (qf.graphics == qf.present) {
        queue_infos.push_back({
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = qf.graphics.value(),
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        });
    } else {
        queue_infos.push_back({
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = qf.graphics.value(),
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        });
        queue_infos.push_back({
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = qf.present.value(),
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        });
    }

    const std::vector<const char*> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = (uint32_t)queue_infos.size(),
        .pQueueCreateInfos = queue_infos.data(),
        .enabledExtensionCount = (uint32_t)device_extensions.size(),
        .ppEnabledExtensionNames = device_extensions.data(),
    };

    VkDevice device;
    if (vkCreateDevice(physical, &device_info, nullptr, &device) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create logical device\n");
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("Logical device created.\n");

    VkQueue graphics_queue, present_queue;
    vkGetDeviceQueue(device, qf.graphics.value(), 0, &graphics_queue);
    vkGetDeviceQueue(device, qf.present.value(), 0, &present_queue);

    // --- Swapchain ---
    SwapchainSupport sw_support = SwapchainSupport::query(physical, surface);
    VkSurfaceFormatKHR sw_format = sw_support.choose_format();
    VkPresentModeKHR sw_present_mode = sw_support.choose_present_mode();
    VkExtent2D sw_extent = sw_support.choose_extent();

    uint32_t image_count = sw_support.caps.minImageCount + 1;
    if (sw_support.caps.maxImageCount > 0 && image_count > sw_support.caps.maxImageCount)
        image_count = sw_support.caps.maxImageCount;

    VkSwapchainCreateInfoKHR sw_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = image_count,
        .imageFormat = sw_format.format,
        .imageColorSpace = sw_format.colorSpace,
        .imageExtent = sw_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = sw_support.caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = sw_present_mode,
        .clipped = VK_TRUE,
    };

    uint32_t qf_indices[] = { qf.graphics.value(), qf.present.value() };
    if (qf.graphics != qf.present) {
        sw_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        sw_info.queueFamilyIndexCount = 2;
        sw_info.pQueueFamilyIndices = qf_indices;
    } else {
        sw_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VkSwapchainKHR swapchain;
    if (vkCreateSwapchainKHR(device, &sw_info, nullptr, &swapchain) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create swapchain\n");
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("Swapchain created (%dx%d).\n", sw_extent.width, sw_extent.height);

    // --- Image Views ---
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);
    std::vector<VkImage> sw_images(image_count);
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, sw_images.data());

    std::vector<VkImageView> sw_image_views(image_count);
    for (uint32_t i = 0; i < image_count; i++) {
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = sw_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = sw_format.format,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        };
        vkCreateImageView(device, &view_info, nullptr, &sw_image_views[i]);
    }

    // --- Render Pass (color + depth) ---
    VkAttachmentDescription color_att = {
        .format = sw_format.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentDescription depth_att = {
        .format = VK_FORMAT_D32_SFLOAT,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference color_ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference depth_ref = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
        .pDepthStencilAttachment = &depth_ref,
    };

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                      | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                      | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                       | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo rp_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 2,
        .pAttachments = &color_att,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    VkRenderPass render_pass;
    vkCreateRenderPass(device, &rp_info, nullptr, &render_pass);

    // ====================================================================
    // Camera UBO — shared by all pipelines (proj + view matrices, 128 bytes)
    // ====================================================================

    // Camera UBO buffer (128 bytes: mat4 proj + mat4 view)
    VkBuffer camera_ubo = VK_NULL_HANDLE;
    VkDeviceMemory camera_ubo_mem = VK_NULL_HANDLE;

    {
        VkBufferCreateInfo ubo_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = 128,  // 2x 4x4 float matrices
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        vkCreateBuffer(device, &ubo_info, nullptr, &camera_ubo);

        VkMemoryRequirements ubo_mem_req;
        vkGetBufferMemoryRequirements(device, camera_ubo, &ubo_mem_req);

        uint32_t ubo_mem_type = find_host_mem_type(physical, ubo_mem_req.memoryTypeBits);
        if (ubo_mem_type == UINT32_MAX) {
            fprintf(stderr, "No host-visible memory for camera UBO\n");
            vkDestroyBuffer(device, camera_ubo, nullptr);
            camera_ubo = VK_NULL_HANDLE;
        } else {
            VkMemoryAllocateInfo ubo_alloc = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = ubo_mem_req.size,
                .memoryTypeIndex = ubo_mem_type,
            };
            vkAllocateMemory(device, &ubo_alloc, nullptr, &camera_ubo_mem);
            vkBindBufferMemory(device, camera_ubo, camera_ubo_mem, 0);
        }
    }

    // Descriptor set layout (binding 0: uniform buffer for vertex shader)
    VkDescriptorSetLayoutBinding dsl_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    };

    VkDescriptorSetLayoutCreateInfo dsl_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &dsl_binding,
    };

    VkDescriptorSetLayout desc_set_layout;
    vkCreateDescriptorSetLayout(device, &dsl_info, nullptr, &desc_set_layout);

    // Descriptor pool
    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
    };

    VkDescriptorPoolCreateInfo dp_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
    };

    VkDescriptorPool desc_pool;
    vkCreateDescriptorPool(device, &dp_info, nullptr, &desc_pool);

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo ds_alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = desc_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &desc_set_layout,
    };

    VkDescriptorSet desc_set;
    vkAllocateDescriptorSets(device, &ds_alloc, &desc_set);

    // Bind UBO to descriptor set
    if (camera_ubo != VK_NULL_HANDLE) {
        VkDescriptorBufferInfo ubo_buf_info = {
            .buffer = camera_ubo,
            .offset = 0,
            .range = 64,
        };

        VkWriteDescriptorSet write_ds = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = desc_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &ubo_buf_info,
        };

        vkUpdateDescriptorSets(device, 1, &write_ds, 0, nullptr);
    }

    // ====================================================================
    // Pipeline: triangle demo
    // ====================================================================
    auto vert_code = load_spv("src/shaders/triangle.vert.spv");
    auto frag_code = load_spv("src/shaders/triangle.frag.spv");
    if (vert_code.empty() || frag_code.empty()) {
        fprintf(stderr, "SPIR-V shaders not found. Build with cmake first.\n");
        vkDestroyDescriptorPool(device, desc_pool, nullptr);
        vkDestroyDescriptorSetLayout(device, desc_set_layout, nullptr);
        if (camera_ubo != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, camera_ubo, nullptr);
            vkFreeMemory(device, camera_ubo_mem, nullptr);
        }
        vkDestroyRenderPass(device, render_pass, nullptr);
        for (auto& v : sw_image_views) vkDestroyImageView(device, v, nullptr);
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    VkShaderModule vert_mod = create_shader_module(device, vert_code);
    VkShaderModule frag_mod = create_shader_module(device, frag_code);

    VkPipelineShaderStageCreateInfo vert_stage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vert_mod,
        .pName = "main",
    };
    VkPipelineShaderStageCreateInfo frag_stage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = frag_mod,
        .pName = "main",
    };
    VkPipelineShaderStageCreateInfo shader_stages[] = { vert_stage, frag_stage };

    // No vertex buffers — use gl_VertexIndex in shader
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .vertexAttributeDescriptionCount = 0,
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkViewport viewport = { 0, 0, (float)sw_extent.width, (float)sw_extent.height, 0.0f, 1.0f };
    VkRect2D scissor = { {0, 0}, sw_extent };

    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
    };

    VkPipelineColorBlendAttachmentState blend_att = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo color_blend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blend_att,
    };

    VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = 3 * sizeof(float),
    };

    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &desc_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_range,
    };

    VkPipelineLayout pipeline_layout;
    vkCreatePipelineLayout(device, &layout_info, nullptr, &pipeline_layout);

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisample,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &color_blend,
        .layout = pipeline_layout,
        .renderPass = render_pass,
        .subpass = 0,
    };

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create pipeline\n");
        vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
        vkDestroyShaderModule(device, frag_mod, nullptr);
        vkDestroyShaderModule(device, vert_mod, nullptr);
        vkDestroyDescriptorPool(device, desc_pool, nullptr);
        vkDestroyDescriptorSetLayout(device, desc_set_layout, nullptr);
        if (camera_ubo != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, camera_ubo, nullptr);
            vkFreeMemory(device, camera_ubo_mem, nullptr);
        }
        vkDestroyRenderPass(device, render_pass, nullptr);
        for (auto& v : sw_image_views) vkDestroyImageView(device, v, nullptr);
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("Pipeline created.\n");

    vkDestroyShaderModule(device, frag_mod, nullptr);
    vkDestroyShaderModule(device, vert_mod, nullptr);

    // --- Line pipeline (VK_PRIMITIVE_TOPOLOGY_LINE_STRIP with primitive restart) ---
    VkPipeline line_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout line_pipeline_layout = VK_NULL_HANDLE;
    VkBuffer line_vb = VK_NULL_HANDLE;
    VkDeviceMemory line_buf_mem = VK_NULL_HANDLE;
    VkBuffer line_ib = VK_NULL_HANDLE;

    bool has_lines = !line_batch.vertices.empty() && !line_batch.indices.empty();

    if (has_lines) {
        // Vertex buffer
        VkDeviceSize vb_size = line_batch.vertices.size() * sizeof(render::LineVertex);
        VkBufferCreateInfo vb_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = vb_size,
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        vkCreateBuffer(device, &vb_info, nullptr, &line_vb);

        VkMemoryRequirements vb_mem_req;
        vkGetBufferMemoryRequirements(device, line_vb, &vb_mem_req);

        // Index buffer
        VkDeviceSize ib_size = line_batch.indices.size() * sizeof(uint32_t);
        VkBufferCreateInfo ib_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = ib_size,
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        vkCreateBuffer(device, &ib_info, nullptr, &line_ib);

        VkMemoryRequirements ib_mem_req;
        vkGetBufferMemoryRequirements(device, line_ib, &ib_mem_req);

        // Allocate combined memory for both buffers
        VkDeviceSize combined_size = vb_mem_req.size + ib_mem_req.size;
        VkDeviceSize ib_offset = (vb_mem_req.size + ib_mem_req.alignment - 1)
                                 & ~(ib_mem_req.alignment - 1);
        combined_size = ib_offset + ib_mem_req.size;

        uint32_t mem_type_idx = find_host_mem_type(physical, vb_mem_req.memoryTypeBits);
        if (mem_type_idx == UINT32_MAX) {
            fprintf(stderr, "No host-visible memory for line buffers\n");
            has_lines = false;
        } else {
            VkMemoryAllocateInfo alloc_info = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = combined_size,
                .memoryTypeIndex = mem_type_idx,
            };
            vkAllocateMemory(device, &alloc_info, nullptr, &line_buf_mem);
            vkBindBufferMemory(device, line_vb, line_buf_mem, 0);
            vkBindBufferMemory(device, line_ib, line_buf_mem, ib_offset);

            // Upload vertex data
            void* mapped;
            vkMapMemory(device, line_buf_mem, 0, vb_size, 0, &mapped);
            memcpy(mapped, line_batch.vertices.data(), vb_size);
            vkUnmapMemory(device, line_buf_mem);

            // Upload index data
            vkMapMemory(device, line_buf_mem, ib_offset, ib_size, 0, &mapped);
            memcpy(mapped, line_batch.indices.data(), ib_size);
            vkUnmapMemory(device, line_buf_mem);
        }

        if (has_lines) {
            // Line shaders
            auto line_vert_code = load_spv("src/shaders/line.vert.spv");
            auto line_frag_code = load_spv("src/shaders/line.frag.spv");

            VkShaderModule line_vert_mod = create_shader_module(device, line_vert_code);
            VkShaderModule line_frag_mod = create_shader_module(device, line_frag_code);

            VkPipelineShaderStageCreateInfo line_vert_stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = line_vert_mod,
                .pName = "main",
            };
            VkPipelineShaderStageCreateInfo line_frag_stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = line_frag_mod,
                .pName = "main",
            };
            VkPipelineShaderStageCreateInfo line_stages[] = { line_vert_stage, line_frag_stage };

            // Vertex input: binding 0 = vec2 position
            VkVertexInputBindingDescription line_binding = {
                .binding = 0,
                .stride = sizeof(render::LineVertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            };
            VkVertexInputAttributeDescription line_attr = {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = 0,
            };
            VkPipelineVertexInputStateCreateInfo line_vertex_input = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &line_binding,
                .vertexAttributeDescriptionCount = 1,
                .pVertexAttributeDescriptions = &line_attr,
            };

            VkPipelineInputAssemblyStateCreateInfo line_input_assembly = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
                .primitiveRestartEnable = VK_TRUE,
            };

            // Same push constant layout (vec3 color) + descriptor set
            VkPushConstantRange line_push_range = {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .offset = 0,
                .size = 3 * sizeof(float),
            };
            VkPipelineLayoutCreateInfo line_layout_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = 1,
                .pSetLayouts = &desc_set_layout,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &line_push_range,
            };
            vkCreatePipelineLayout(device, &line_layout_info, nullptr, &line_pipeline_layout);

            VkPipelineRasterizationStateCreateInfo line_rasterizer = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_NONE,
                .frontFace = VK_FRONT_FACE_CLOCKWISE,
                .lineWidth = 1.0f,
            };

            VkGraphicsPipelineCreateInfo line_pipeline_info = {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .stageCount = 2,
                .pStages = line_stages,
                .pVertexInputState = &line_vertex_input,
                .pInputAssemblyState = &line_input_assembly,
                .pViewportState = &viewport_state,
                .pRasterizationState = &line_rasterizer,
                .pMultisampleState = &multisample,
                .pDepthStencilState = &depth_stencil,
                .pColorBlendState = &color_blend,
                .layout = line_pipeline_layout,
                .renderPass = render_pass,
                .subpass = 0,
            };

            if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &line_pipeline_info,
                                          nullptr, &line_pipeline) != VK_SUCCESS) {
                fprintf(stderr, "Failed to create line pipeline\n");
                has_lines = false;
            } else {
                printf("Line pipeline created.\n");
            }

            vkDestroyShaderModule(device, line_frag_mod, nullptr);
            vkDestroyShaderModule(device, line_vert_mod, nullptr);
        }
    }

    // --- Fill pipeline (polygon fills, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) ---
    VkPipeline fill_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout fill_pipeline_layout = VK_NULL_HANDLE;
    VkBuffer fill_vb = VK_NULL_HANDLE;
    VkDeviceMemory fill_buf_mem = VK_NULL_HANDLE;
    VkBuffer fill_ib = VK_NULL_HANDLE;

    bool has_fills = !poly_batch.vertices.empty() && !poly_batch.indices.empty();

    if (has_fills) {
        // Vertex buffer
        VkDeviceSize fvb_size = poly_batch.vertices.size() * sizeof(render::PolyVertex);
        VkBufferCreateInfo fvb_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = fvb_size,
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        vkCreateBuffer(device, &fvb_info, nullptr, &fill_vb);

        VkMemoryRequirements fvb_mem_req;
        vkGetBufferMemoryRequirements(device, fill_vb, &fvb_mem_req);

        // Index buffer
        VkDeviceSize fib_size = poly_batch.indices.size() * sizeof(uint32_t);
        VkBufferCreateInfo fib_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = fib_size,
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        vkCreateBuffer(device, &fib_info, nullptr, &fill_ib);

        VkMemoryRequirements fib_mem_req;
        vkGetBufferMemoryRequirements(device, fill_ib, &fib_mem_req);

        // Allocate combined memory
        VkDeviceSize fcombined_size = fvb_mem_req.size + fib_mem_req.size;
        VkDeviceSize fib_offset = (fvb_mem_req.size + fib_mem_req.alignment - 1)
                                   & ~(fib_mem_req.alignment - 1);
        fcombined_size = fib_offset + fib_mem_req.size;

        uint32_t fmem_type_idx = find_host_mem_type(physical, fvb_mem_req.memoryTypeBits);
        if (fmem_type_idx == UINT32_MAX) {
            fprintf(stderr, "No host-visible memory for fill buffers\n");
            has_fills = false;
        } else {
            VkMemoryAllocateInfo falloc_info = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = fcombined_size,
                .memoryTypeIndex = fmem_type_idx,
            };
            vkAllocateMemory(device, &falloc_info, nullptr, &fill_buf_mem);
            vkBindBufferMemory(device, fill_vb, fill_buf_mem, 0);
            vkBindBufferMemory(device, fill_ib, fill_buf_mem, fib_offset);

            // Upload vertex data
            void* fmapped;
            vkMapMemory(device, fill_buf_mem, 0, fvb_size, 0, &fmapped);
            memcpy(fmapped, poly_batch.vertices.data(), fvb_size);
            vkUnmapMemory(device, fill_buf_mem);

            // Upload index data
            vkMapMemory(device, fill_buf_mem, fib_offset, fib_size, 0, &fmapped);
            memcpy(fmapped, poly_batch.indices.data(), fib_size);
            vkUnmapMemory(device, fill_buf_mem);
        }

        if (has_fills) {
            // Fill shaders
            auto fill_vert_code = load_spv("src/shaders/fill.vert.spv");
            auto fill_frag_code = load_spv("src/shaders/fill.frag.spv");

            VkShaderModule fill_vert_mod = create_shader_module(device, fill_vert_code);
            VkShaderModule fill_frag_mod = create_shader_module(device, fill_frag_code);

            VkPipelineShaderStageCreateInfo fill_vert_stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = fill_vert_mod,
                .pName = "main",
            };
            VkPipelineShaderStageCreateInfo fill_frag_stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = fill_frag_mod,
                .pName = "main",
            };
            VkPipelineShaderStageCreateInfo fill_stages[] = { fill_vert_stage, fill_frag_stage };

            // Vertex input: binding 0 = vec2 position
            VkVertexInputBindingDescription fill_binding = {
                .binding = 0,
                .stride = sizeof(render::PolyVertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            };
            VkVertexInputAttributeDescription fill_attr = {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = 0,
            };
            VkPipelineVertexInputStateCreateInfo fill_vertex_input = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &fill_binding,
                .vertexAttributeDescriptionCount = 1,
                .pVertexAttributeDescriptions = &fill_attr,
            };

            VkPipelineInputAssemblyStateCreateInfo fill_input_assembly = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                .primitiveRestartEnable = VK_FALSE,
            };

            // Push constant: vec4 (rgb + opacity) = 16 bytes + descriptor set
            VkPushConstantRange fill_push_range = {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .offset = 0,
                .size = 4 * sizeof(float),
            };
            VkPipelineLayoutCreateInfo fill_layout_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = 1,
                .pSetLayouts = &desc_set_layout,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &fill_push_range,
            };
            vkCreatePipelineLayout(device, &fill_layout_info, nullptr, &fill_pipeline_layout);

            VkPipelineRasterizationStateCreateInfo fill_rasterizer = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_NONE,
                .frontFace = VK_FRONT_FACE_CLOCKWISE,
                .lineWidth = 1.0f,
            };

            VkGraphicsPipelineCreateInfo fill_pipeline_info = {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .stageCount = 2,
                .pStages = fill_stages,
                .pVertexInputState = &fill_vertex_input,
                .pInputAssemblyState = &fill_input_assembly,
                .pViewportState = &viewport_state,
                .pRasterizationState = &fill_rasterizer,
                .pMultisampleState = &multisample,
                .pDepthStencilState = &depth_stencil,
                .pColorBlendState = &color_blend,
                .layout = fill_pipeline_layout,
                .renderPass = render_pass,
                .subpass = 0,
            };

            if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &fill_pipeline_info,
                                          nullptr, &fill_pipeline) != VK_SUCCESS) {
                fprintf(stderr, "Failed to create fill pipeline\n");
                has_fills = false;
            } else {
                printf("Fill pipeline created.\n");
            }

            vkDestroyShaderModule(device, fill_frag_mod, nullptr);
            vkDestroyShaderModule(device, fill_vert_mod, nullptr);
        }
    }

    // --- Building pipeline (3D extruded buildings) ---
    VkPipeline building_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout building_pipeline_layout = VK_NULL_HANDLE;
    VkBuffer building_vb = VK_NULL_HANDLE;
    VkDeviceMemory building_buf_mem = VK_NULL_HANDLE;
    VkBuffer building_ib = VK_NULL_HANDLE;

    bool has_buildings = !buildingBatch.vertices.empty() && !buildingBatch.indices.empty();

    if (has_buildings) {
        // Vertex buffer
        VkDeviceSize bvb_size = buildingBatch.vertices.size() * sizeof(bldg::BuildingVertex);
        VkBufferCreateInfo bvb_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = bvb_size,
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        vkCreateBuffer(device, &bvb_info, nullptr, &building_vb);

        VkMemoryRequirements bvb_mem_req;
        vkGetBufferMemoryRequirements(device, building_vb, &bvb_mem_req);

        // Index buffer
        VkDeviceSize bib_size = buildingBatch.indices.size() * sizeof(uint32_t);
        VkBufferCreateInfo bib_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = bib_size,
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        vkCreateBuffer(device, &bib_info, nullptr, &building_ib);

        VkMemoryRequirements bib_mem_req;
        vkGetBufferMemoryRequirements(device, building_ib, &bib_mem_req);

        // Allocate combined memory
        VkDeviceSize bcombined_size = bvb_mem_req.size + bib_mem_req.size;
        VkDeviceSize bib_offset = (bvb_mem_req.size + bib_mem_req.alignment - 1)
                                   & ~(bib_mem_req.alignment - 1);
        bcombined_size = bib_offset + bib_mem_req.size;

        uint32_t bmem_type_idx = find_host_mem_type(physical, bvb_mem_req.memoryTypeBits);
        if (bmem_type_idx == UINT32_MAX) {
            fprintf(stderr, "No host-visible memory for building buffers\n");
            has_buildings = false;
        } else {
            VkMemoryAllocateInfo balloc_info = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = bcombined_size,
                .memoryTypeIndex = bmem_type_idx,
            };
            vkAllocateMemory(device, &balloc_info, nullptr, &building_buf_mem);
            vkBindBufferMemory(device, building_vb, building_buf_mem, 0);
            vkBindBufferMemory(device, building_ib, building_buf_mem, bib_offset);

            // Upload vertex data
            void* bmapped;
            vkMapMemory(device, building_buf_mem, 0, bvb_size, 0, &bmapped);
            memcpy(bmapped, buildingBatch.vertices.data(), bvb_size);
            vkUnmapMemory(device, building_buf_mem);

            // Upload index data
            vkMapMemory(device, building_buf_mem, bib_offset, bib_size, 0, &bmapped);
            memcpy(bmapped, buildingBatch.indices.data(), bib_size);
            vkUnmapMemory(device, building_buf_mem);
        }

        if (has_buildings) {
            // Building shaders
            auto bvert_code = load_spv("src/shaders/building.vert.spv");
            auto bfrag_code = load_spv("src/shaders/building.frag.spv");

            VkShaderModule bvert_mod = create_shader_module(device, bvert_code);
            VkShaderModule bfrag_mod = create_shader_module(device, bfrag_code);

            VkPipelineShaderStageCreateInfo bvert_stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = bvert_mod,
                .pName = "main",
            };
            VkPipelineShaderStageCreateInfo bfrag_stage = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = bfrag_mod,
                .pName = "main",
            };
            VkPipelineShaderStageCreateInfo bstages[] = { bvert_stage, bfrag_stage };

            // Vertex input: binding 0 = vec3 position
            VkVertexInputBindingDescription b_binding = {
                .binding = 0,
                .stride = sizeof(bldg::BuildingVertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            };
            VkVertexInputAttributeDescription b_attr = {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = 0,
            };
            VkPipelineVertexInputStateCreateInfo b_vertex_input = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &b_binding,
                .vertexAttributeDescriptionCount = 1,
                .pVertexAttributeDescriptions = &b_attr,
            };

            VkPipelineInputAssemblyStateCreateInfo b_input_assembly = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                .primitiveRestartEnable = VK_FALSE,
            };

            // Push constant: vec4 (rgba) = 16 bytes
            VkPushConstantRange b_push_range = {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .offset = 0,
                .size = 4 * sizeof(float),
            };
            VkPipelineLayoutCreateInfo b_layout_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = 1,
                .pSetLayouts = &desc_set_layout,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &b_push_range,
            };
            vkCreatePipelineLayout(device, &b_layout_info, nullptr, &building_pipeline_layout);

            VkPipelineRasterizationStateCreateInfo b_rasterizer = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_BACK_BIT,
                .frontFace = VK_FRONT_FACE_CLOCKWISE,
                .lineWidth = 1.0f,
            };

            VkGraphicsPipelineCreateInfo b_pipeline_info = {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .stageCount = 2,
                .pStages = bstages,
                .pVertexInputState = &b_vertex_input,
                .pInputAssemblyState = &b_input_assembly,
                .pViewportState = &viewport_state,
                .pRasterizationState = &b_rasterizer,
                .pMultisampleState = &multisample,
                .pDepthStencilState = &depth_stencil,
                .pColorBlendState = &color_blend,
                .layout = building_pipeline_layout,
                .renderPass = render_pass,
                .subpass = 0,
            };

            if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &b_pipeline_info,
                                          nullptr, &building_pipeline) != VK_SUCCESS) {
                fprintf(stderr, "Failed to create building pipeline\n");
                has_buildings = false;
            } else {
                printf("Building pipeline created.\n");
            }

            vkDestroyShaderModule(device, bfrag_mod, nullptr);
            vkDestroyShaderModule(device, bvert_mod, nullptr);
        }
    }

    // --- Depth buffer image ---
    VkImage depth_image = VK_NULL_HANDLE;
    VkDeviceMemory depth_mem = VK_NULL_HANDLE;
    VkImageView depth_view = VK_NULL_HANDLE;

    {
        VkImageCreateInfo depth_img_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_D32_SFLOAT,
            .extent = { sw_extent.width, sw_extent.height, 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };
        vkCreateImage(device, &depth_img_info, nullptr, &depth_image);

        VkMemoryRequirements depth_mem_req;
        vkGetImageMemoryRequirements(device, depth_image, &depth_mem_req);

        uint32_t depth_mem_type = find_host_mem_type(physical, depth_mem_req.memoryTypeBits);
        if (depth_mem_type != UINT32_MAX) {
            VkMemoryAllocateInfo depth_alloc = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = depth_mem_req.size,
                .memoryTypeIndex = depth_mem_type,
            };
            vkAllocateMemory(device, &depth_alloc, nullptr, &depth_mem);
            vkBindImageMemory(device, depth_image, depth_mem, 0);

            VkImageViewCreateInfo depth_view_info = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = depth_image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = VK_FORMAT_D32_SFLOAT,
                .subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 },
            };
            vkCreateImageView(device, &depth_view_info, nullptr, &depth_view);
        } else {
            fprintf(stderr, "Warning: no device memory for depth buffer, depth testing disabled\n");
        }
    }

    // --- Framebuffers (with depth attachment) ---
    std::vector<VkFramebuffer> framebuffers(image_count);
    for (uint32_t i = 0; i < image_count; i++) {
        VkImageView attachments[2] = { sw_image_views[i], depth_view };
        VkFramebufferCreateInfo fb_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass,
            .attachmentCount = 2,
            .pAttachments = attachments,
            .width = sw_extent.width,
            .height = sw_extent.height,
            .layers = 1,
        };
        vkCreateFramebuffer(device, &fb_info, nullptr, &framebuffers[i]);
    }

    // --- Command Pool & Buffers ---
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = qf.graphics.value(),
    };
    VkCommandPool cmd_pool;
    vkCreateCommandPool(device, &pool_info, nullptr, &cmd_pool);

    std::vector<VkCommandBuffer> cmd_bufs(image_count);
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = image_count,
    };
    vkAllocateCommandBuffers(device, &alloc_info, cmd_bufs.data());

    // Helper lambda to record command buffer for a given image index
    auto record_command_buffer = [&](uint32_t i) {
        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        };
        vkBeginCommandBuffer(cmd_bufs[i], &begin_info);

        VkClearValue clear[2] = {
            { .color = { {0.02f, 0.02f, 0.06f, 1.0f} } },
            { .depthStencil = { 1.0f, 0 } }
        };
        VkRenderPassBeginInfo rp_begin = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = render_pass,
            .framebuffer = framebuffers[i],
            .renderArea = { {0, 0}, sw_extent },
            .clearValueCount = 2,
            .pClearValues = clear,
        };
        vkCmdBeginRenderPass(cmd_bufs[i], &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

        // Triangle demo (always rendered)
        vkCmdBindDescriptorSets(cmd_bufs[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline_layout, 0, 1, &desc_set, 0, nullptr);
        vkCmdBindPipeline(cmd_bufs[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdPushConstants(cmd_bufs[i], pipeline_layout,
                          VK_SHADER_STAGE_VERTEX_BIT, 0,
                          3 * sizeof(float), rule.fill_color);
        vkCmdDraw(cmd_bufs[i], 3, 1, 0, 0);

        // Draw 2D fills (parks, water, landuse) - only in 2D mode
        if (mode == 1 && !fills2d.empty()) {
            for (const auto& [polygon, color] : fills2d) {
                if (polygon.size() < 3) continue;

                // Convert polygon to clip-space vertices and indices
                std::vector<render::PolyVertex> fill_verts;
                std::vector<uint32_t> fill_indices;
                float inv_extent = 1.0f / 65536.0f;  // world_width

                uint32_t base = 0;
                for (size_t j = 0; j + 2 < polygon.size(); ++j) {
                    float clip_x0 = (polygon[0].x * inv_extent) * 2.0f - 1.0f;
                    float clip_y0 = 1.0f - (polygon[0].y * inv_extent) * 2.0f;
                    float clip_x1 = (polygon[j].x * inv_extent) * 2.0f - 1.0f;
                    float clip_y1 = 1.0f - (polygon[j].y * inv_extent) * 2.0f;
                    float clip_x2 = (polygon[j+1].x * inv_extent) * 2.0f - 1.0f;
                    float clip_y2 = 1.0f - (polygon[j+1].y * inv_extent) * 2.0f;

                    fill_verts.push_back({clip_x0, clip_y0});
                    fill_verts.push_back({clip_x1, clip_y1});
                    fill_verts.push_back({clip_x2, clip_y2});

                    uint32_t vbase = static_cast<uint32_t>(fill_verts.size()) - 3;
                    fill_indices.push_back(vbase);
                    fill_indices.push_back(vbase + 1);
                    fill_indices.push_back(vbase + 2);
                }

                if (fill_verts.empty()) continue;

                // Upload fill data to a temporary buffer (simplified: use existing fill_vb)
                // For now, skip individual polygon rendering and use batched approach
                style::StyleRule fill_rule = styleEngine.matchRule("buildings", std::string("fill"));
                float fill_pc[4] = { color.r, color.g, color.b, 0.7f };

                vkCmdBindDescriptorSets(cmd_bufs[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        fill_pipeline_layout, 0, 1, &desc_set, 0, nullptr);
                vkCmdBindPipeline(cmd_bufs[i], VK_PIPELINE_BIND_POINT_GRAPHICS, fill_pipeline);
                VkDeviceSize foffsets[] = {0};
                vkCmdBindVertexBuffers(cmd_bufs[i], 0, 1, &fill_vb, foffsets);
                if (fill_ib != VK_NULL_HANDLE) {
                    vkCmdBindIndexBuffer(cmd_bufs[i], fill_ib, 0, VK_INDEX_TYPE_UINT32);
                    vkCmdPushConstants(cmd_bufs[i], fill_pipeline_layout,
                                      VK_SHADER_STAGE_VERTEX_BIT, 0,
                                      4 * sizeof(float), fill_pc);
                    vkCmdDrawIndexed(cmd_bufs[i],
                                     static_cast<uint32_t>(poly_batch.indices.size()),
                                     1, 0, 0, 0);
                }
            }
        }

        // Draw buildings (3D) - only in 3D mode
        if (mode == 2 && has_buildings) {
            style::StyleRule buildRule = styleEngine.matchRule("buildings", std::string("fill-extrusion"));
            float build_pc[4] = { buildRule.extrude_color[0], buildRule.extrude_color[1],
                                  buildRule.extrude_color[2], buildRule.extrude_opacity };

            DEBUG_LOG("Rendering buildings: %zu vertices, %u indices",
                      buildingBatch.vertices.size(), buildingBatch.indices.size());
            DEBUG_LOG("  Build color: rgba(%.2f, %.2f, %.2f, %.2f)",
                      build_pc[0], build_pc[1], build_pc[2], build_pc[3]);

            vkCmdBindDescriptorSets(cmd_bufs[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    building_pipeline_layout, 0, 1, &desc_set, 0, nullptr);
            vkCmdBindPipeline(cmd_bufs[i], VK_PIPELINE_BIND_POINT_GRAPHICS, building_pipeline);
            VkDeviceSize boffsets[] = {0};
            vkCmdBindVertexBuffers(cmd_bufs[i], 0, 1, &building_vb, boffsets);
            if (building_ib != VK_NULL_HANDLE) {
                vkCmdBindIndexBuffer(cmd_bufs[i], building_ib, 0, VK_INDEX_TYPE_UINT32);
                vkCmdPushConstants(cmd_bufs[i], building_pipeline_layout,
                                  VK_SHADER_STAGE_VERTEX_BIT, 0,
                                  4 * sizeof(float), build_pc);
                vkCmdDrawIndexed(cmd_bufs[i],
                                 static_cast<uint32_t>(buildingBatch.indices.size()),
                                 1, 0, 0, 0);
                printf("Frame: rendered %u building indices\n", buildingBatch.indices.size());
            }
        }

        // Draw lines on top
        if (has_lines) {
            style::StyleRule line_rule = styleEngine.matchRule("streets", std::string("line"));
            vkCmdBindDescriptorSets(cmd_bufs[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    line_pipeline_layout, 0, 1, &desc_set, 0, nullptr);
            vkCmdBindPipeline(cmd_bufs[i], VK_PIPELINE_BIND_POINT_GRAPHICS, line_pipeline);
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd_bufs[i], 0, 1, &line_vb, offsets);
            if (line_ib != VK_NULL_HANDLE) {
                vkCmdBindIndexBuffer(cmd_bufs[i], line_ib, 0, VK_INDEX_TYPE_UINT32);
                vkCmdPushConstants(cmd_bufs[i], line_pipeline_layout,
                                  VK_SHADER_STAGE_VERTEX_BIT, 0,
                                  3 * sizeof(float), line_rule.line_color);
                vkCmdDrawIndexed(cmd_bufs[i],
                                 static_cast<uint32_t>(line_batch.indices.size()),
                                 1, 0, 0, 0);
            }
        }

        vkCmdEndRenderPass(cmd_bufs[i]);
        vkEndCommandBuffer(cmd_bufs[i]);
    };

    // --- Sync objects ---
    VkSemaphoreCreateInfo sem_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    VkSemaphore image_available, render_finished;
    VkFence in_flight;
    vkCreateSemaphore(device, &sem_info, nullptr, &image_available);
    vkCreateSemaphore(device, &sem_info, nullptr, &render_finished);
    vkCreateFence(device, &fence_info, nullptr, &in_flight);

    // ====================================================================
    // Main loop with interactive camera (milestone 7)
    // ====================================================================
    printf("Rendering. Arrow keys=pan, +/-=zoom, left-drag=pan, scroll=zoom, ESC=quit.\n");
    printf("Camera: pos=(%.2f,%.2f) zoom=%.1f\n", camera_x, camera_y, zoom_level);

    bool running = true;
    bool mouse_dragging = false;
    int last_mouse_x = 0, last_mouse_y = 0;

    while (running) {
        // --- Handle SDL events ---
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT:
                running = false;
                break;

            case SDL_KEYDOWN:
                if (ev.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                } else if (ev.key.keysym.sym == SDLK_F1) {
                    mode = 1;
                    printf("Mode: 2D\n");
                } else if (ev.key.keysym.sym == SDLK_F2) {
                    mode = 2;
                    printf("Mode: 3D\n");
                } else if (ev.key.keysym.sym == SDLK_EQUALS || ev.key.keysym.sym == SDLK_PLUS) {
                    // Zoom in
                    zoom_level = clamp_val(zoom_level + 1.0f, 2.0f, 18.0f);
                    printf("Camera: pos=(%.2f,%.2f) zoom=%.1f mode=%d\n", camera_x, camera_y, zoom_level, mode);
                } else if (ev.key.keysym.sym == SDLK_MINUS) {
                    // Zoom out
                    zoom_level = clamp_val(zoom_level - 1.0f, 2.0f, 18.0f);
                    printf("Camera: pos=(%.2f,%.2f) zoom=%.1f mode=%d\n", camera_x, camera_y, zoom_level, mode);
                } else if (ev.key.keysym.sym == SDLK_LEFT) {
                    // Pan left
                    float pan_step = 0.05f * (2.0f / zoom_level);
                    camera_x -= pan_step;
                    printf("Camera: pos=(%.2f,%.2f) zoom=%.1f mode=%d\n", camera_x, camera_y, zoom_level, mode);
                } else if (ev.key.keysym.sym == SDLK_RIGHT) {
                    float pan_step = 0.05f * (2.0f / zoom_level);
                    camera_x += pan_step;
                    printf("Camera: pos=(%.2f,%.2f) zoom=%.1f mode=%d\n", camera_x, camera_y, zoom_level, mode);
                } else if (ev.key.keysym.sym == SDLK_UP) {
                    float pan_step = 0.05f * (2.0f / zoom_level);
                    camera_y -= pan_step;
                    printf("Camera: pos=(%.2f,%.2f) zoom=%.1f mode=%d\n", camera_x, camera_y, zoom_level, mode);
                } else if (ev.key.keysym.sym == SDLK_DOWN) {
                    float pan_step = 0.05f * (2.0f / zoom_level);
                    camera_y += pan_step;
                    printf("Camera: pos=(%.2f,%.2f) zoom=%.1f mode=%d\n", camera_x, camera_y, zoom_level, mode);
                } else if (ev.key.keysym.sym == SDLK_q) {
                    // Decrease tilt (more top-down)
                    tilt_angle = clamp_val(tilt_angle - 5.0f, 0.0f, 80.0f);
                    printf("Tilt: %.1f degrees\n", tilt_angle);
                } else if (ev.key.keysym.sym == SDLK_e) {
                    // Increase tilt (more side view)
                    tilt_angle = clamp_val(tilt_angle + 5.0f, 0.0f, 80.0f);
                    printf("Tilt: %.1f degrees\n", tilt_angle);
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    mouse_dragging = true;
                    last_mouse_x = ev.button.x;
                    last_mouse_y = ev.button.y;
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    mouse_dragging = false;
                }
                break;

            case SDL_MOUSEMOTION:
                if (mouse_dragging) {
                    int dx = ev.motion.x - last_mouse_x;
                    int dy = ev.motion.y - last_mouse_y;
                    last_mouse_x = ev.motion.x;
                    last_mouse_y = ev.motion.y;

                    // Convert pixel delta to clip-space delta
                    float aspect = (float)sw_extent.width / (float)sw_extent.height;
                    float visible_half_w = 2.0f / zoom_level;
                    float pixel_to_clip_x = (2.0f * visible_half_w) / (float)sw_extent.width;
                    float pixel_to_clip_y = (2.0f * visible_half_w / aspect) / (float)sw_extent.height;

                    camera_x -= (float)dx * pixel_to_clip_x;
                    camera_y -= (float)dy * pixel_to_clip_y;
                }
                break;

            case SDL_MOUSEWHEEL:
                {
                    float zoom_delta = (ev.wheel.y > 0) ? 1.0f : -1.0f;
                    zoom_level = clamp_val(zoom_level + zoom_delta, 2.0f, 18.0f);
                    printf("Camera: pos=(%.2f,%.2f) zoom=%.1f\n", camera_x, camera_y, zoom_level);
                }
                break;
            }
        }

        // --- Update camera UBO ---
        if (camera_ubo != VK_NULL_HANDLE) {
            float aspect = (float)sw_extent.width / (float)sw_extent.height;
            glm::mat4 proj, view;

            DEBUG_LOG("Camera update: mode=%d, zoom=%.6f, pos=(%.2f,%.2f)", mode, zoom_level, camera_x, camera_y);

            if (mode == 2) {
                // 3D mode: perspective projection with tilt
                // Buildings are at (x, height, z) where x,z are normalized coords (centered at origin)
                // Camera should be above buildings in Y (height) direction
                float fov = 60.0f;
                float near_plane = 1.0f;
                float far_plane = std::max(2000.0f, max_building_height * 10.0f);

                proj = glm::perspective(
                    glm::radians(fov),
                    aspect,
                    near_plane,
                    far_plane
                );

                // View matrix: camera above buildings looking down
                float tilt_rad = glm::radians(tilt_angle);
                // Distance based on actual building dimensions (in meters)
                // Camera must be high enough to see the tallest building
                float building_horizontal_range = std::max(
                    bldg_max_x - bldg_min_x,
                    bldg_max_z - bldg_min_z
                ) * 0.5f;
                float cam_distance = std::max(building_horizontal_range * 2.5f,
                                              max_building_height * 2.0f);
                float cam_height = cam_distance * std::cos(tilt_rad);
                float cam_offset = cam_distance * std::sin(tilt_rad);
                glm::vec3 cam_pos(
                    camera_x,
                    cam_height,
                    camera_y + cam_offset
                );
                glm::vec3 look_at(camera_x, 0.0f, camera_y);
                view = glm::lookAt(cam_pos, look_at, glm::vec3(0.0f, 1.0f, 0.0f));
            } else {
                // 2D mode: orthographic projection
                float visible_half_w = 2.0f / zoom_level;
                float visible_half_h = visible_half_w / aspect;

                proj = glm::ortho(
                    camera_x - visible_half_w,
                    camera_x + visible_half_w,
                    camera_y - visible_half_h,
                    camera_y + visible_half_h,
                    -1.0f, 1.0f
                );

                view = glm::mat4(1.0f);
            }

            void* ubo_mapped;
            vkMapMemory(device, camera_ubo_mem, 0, 128, 0, &ubo_mapped);
            memcpy(ubo_mapped, &proj[0][0], 64);
            memcpy(static_cast<char*>(ubo_mapped) + 64, &view[0][0], 64);
            vkUnmapMemory(device, camera_ubo_mem);
            
            DEBUG_LOG("Camera matrices uploaded (mode=%d)", mode);
            DEBUG_LOG("  proj[0][0]=%.4f proj[1][1]=%.4f proj[2][2]=%.4f", 
                      proj[0][0], proj[1][1], proj[2][2]);
            DEBUG_LOG("  view[3][0]=%.2f view[3][1]=%.2f view[3][2]=%.2f",
                      view[3][0], view[3][1], view[3][2]);
        }

        // --- Render ---
        vkWaitForFences(device, 1, &in_flight, VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &in_flight);

        uint32_t image_idx;
        VkResult acq = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, image_available, VK_NULL_HANDLE, &image_idx);
        if (acq == VK_ERROR_OUT_OF_DATE_KHR) continue;

        // --- Record command buffer ---
        record_command_buffer(image_idx);
        if (acq == VK_ERROR_OUT_OF_DATE_KHR) continue;

        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &image_available,
            .pWaitDstStageMask = &wait_stage,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd_bufs[image_idx],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &render_finished,
        };
        vkQueueSubmit(graphics_queue, 1, &submit_info, in_flight);

        VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &render_finished,
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &image_idx,
        };
        VkResult pres = vkQueuePresentKHR(present_queue, &present_info);
        if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR) continue;
    }

    vkDeviceWaitIdle(device);

    // --- Cleanup ---
    vkDestroySemaphore(device, image_available, nullptr);
    vkDestroySemaphore(device, render_finished, nullptr);
    vkDestroyFence(device, in_flight, nullptr);
    vkDestroyCommandPool(device, cmd_pool, nullptr);
    for (auto& fb : framebuffers) vkDestroyFramebuffer(device, fb, nullptr);
    if (fill_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, fill_pipeline, nullptr);
        vkDestroyPipelineLayout(device, fill_pipeline_layout, nullptr);
    }
    if (fill_vb != VK_NULL_HANDLE) vkDestroyBuffer(device, fill_vb, nullptr);
    if (fill_ib != VK_NULL_HANDLE) vkDestroyBuffer(device, fill_ib, nullptr);
    if (fill_buf_mem != VK_NULL_HANDLE) vkFreeMemory(device, fill_buf_mem, nullptr);
    if (line_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, line_pipeline, nullptr);
        vkDestroyPipelineLayout(device, line_pipeline_layout, nullptr);
    }
    if (line_vb != VK_NULL_HANDLE) vkDestroyBuffer(device, line_vb, nullptr);
    if (line_ib != VK_NULL_HANDLE) vkDestroyBuffer(device, line_ib, nullptr);
    if (line_buf_mem != VK_NULL_HANDLE) vkFreeMemory(device, line_buf_mem, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    vkDestroyRenderPass(device, render_pass, nullptr);
    for (auto& v : sw_image_views) vkDestroyImageView(device, v, nullptr);
    vkDestroySwapchainKHR(device, swapchain, nullptr);

    // Cleanup camera resources
    vkDestroyDescriptorPool(device, desc_pool, nullptr);
    vkDestroyDescriptorSetLayout(device, desc_set_layout, nullptr);
    if (camera_ubo != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, camera_ubo, nullptr);
        vkFreeMemory(device, camera_ubo_mem, nullptr);
    }

    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("Clean exit.\n");
    return 0;
}

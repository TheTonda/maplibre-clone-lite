// building_test.cpp — Test building rendering with correct pipeline setup
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <cstdio>
#include <vector>
#include <fstream>
#include <cstring>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "osm_loader.h"
#include "building_data.h"

constexpr int WIDTH = 1024;
constexpr int HEIGHT = 768;

static std::vector<uint32_t> load_spv(const char* path) {
    std::ifstream f(path, std::ios::ate | std::ios::binary);
    if (!f) {
        fprintf(stderr, "Failed to open: %s\n", path);
        return {};
    }
    size_t sz = f.tellg();
    f.seekg(0);
    std::vector<uint32_t> data(sz / 4);
    f.read(reinterpret_cast<char*>(data.data()), sz);
    return data;
}

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

int main() {
    fprintf(stderr, "BUILDING TEST START\n");
    fflush(stderr);
    
    // Load OSM data
    printf("Loading OSM data...\n");
    osm::OSMData osmData = osm::load_osm_json("data/osm_data.json");
    printf("Loaded %zu buildings\n", osmData.buildings.size());
    
    // Extract building geometry
    printf("Extracting building geometry...\n");
    bldg::BuildingBatch buildingBatch = bldg::extract_buildings(osmData.buildings);
    printf("Generated %zu vertices, %zu indices\n", 
           buildingBatch.vertices.size(), buildingBatch.indices.size());
    
    // Print some sample vertices
    if (!buildingBatch.vertices.empty()) {
        printf("Sample vertex 0: (%.2f, %.2f, %.2f)\n", 
               buildingBatch.vertices[0].x,
               buildingBatch.vertices[0].y,
               buildingBatch.vertices[0].z);
        printf("Sample vertex 1: (%.2f, %.2f, %.2f)\n", 
               buildingBatch.vertices[1].x,
               buildingBatch.vertices[1].y,
               buildingBatch.vertices[1].z);
    }
    
    // Compute bounds
    float min_x = 1e9f, min_y = 1e9f, min_z = 1e9f;
    float max_x = -1e9f, max_y = -1e9f, max_z = -1e9f;
    for (const auto& v : buildingBatch.vertices) {
        if (v.x < min_x) min_x = v.x;
        if (v.y < min_y) min_y = v.y;
        if (v.z < min_z) min_z = v.z;
        if (v.x > max_x) max_x = v.x;
        if (v.y > max_y) max_y = v.y;
        if (v.z > max_z) max_z = v.z;
    }
    printf("Bounds: X(%.0f to %.0f) Y(%.0f to %.0f) Z(%.0f to %.0f)\n",
           min_x, max_x, min_y, max_y, min_z, max_z);
    printf("Size: %.0f x %.0f x %.0f\n",
           max_x - min_x, max_y - min_y, max_z - min_z);
    
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    printf("SDL_Init OK\n");
    
    SDL_Window* window = SDL_CreateWindow(
        "Building Test",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIDTH, HEIGHT,
        SDL_WINDOW_VULKAN
    );
    if (!window) {
        fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    printf("Window OK\n");
    
    // Vulkan instance
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Building Test",
        .apiVersion = VK_API_VERSION_1_3,
    };
    
    unsigned ext_count;
    SDL_Vulkan_GetInstanceExtensions(window, &ext_count, nullptr);
    std::vector<const char*> extensions(ext_count);
    SDL_Vulkan_GetInstanceExtensions(window, &ext_count, extensions.data());
    
    VkInstance instance;
    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = (uint32_t)extensions.size(),
        .ppEnabledExtensionNames = extensions.data(),
    };
    if (vkCreateInstance(&instance_info, nullptr, &instance) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create instance\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
        fprintf(stderr, "Failed to create surface\n");
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    uint32_t device_count;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
    VkPhysicalDevice physical = devices[0];
    
    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, nullptr);
    std::vector<VkQueueFamilyProperties> qfp(count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, qfp.data());
    
    uint32_t graphics_family = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (qfp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_family = i;
            break;
        }
    }
    
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = graphics_family,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };
    
    const std::vector<const char*> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    
    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = (uint32_t)device_extensions.size(),
        .ppEnabledExtensionNames = device_extensions.data(),
    };
    
    VkDevice device;
    vkCreateDevice(physical, &device_info, nullptr, &device);
    VkQueue graphics_queue;
    vkGetDeviceQueue(device, graphics_family, 0, &graphics_queue);
    
    // Swapchain
    VkSurfaceFormatKHR surface_format;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, formats.data());
    surface_format = formats[0];
    
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    VkExtent2D extent = { WIDTH, HEIGHT };
    
    uint32_t image_count;
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps);
    image_count = caps.minImageCount + 1;
    
    VkSwapchainCreateInfoKHR sw_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
    };
    
    VkSwapchainKHR swapchain;
    vkCreateSwapchainKHR(device, &sw_info, nullptr, &swapchain);
    
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);
    std::vector<VkImage> sw_images(image_count);
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, sw_images.data());
    
    std::vector<VkImageView> sw_image_views(image_count);
    for (uint32_t i = 0; i < image_count; i++) {
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = sw_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = surface_format.format,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        };
        vkCreateImageView(device, &view_info, nullptr, &sw_image_views[i]);
    }
    
    // Render pass with depth
    VkAttachmentDescription color_att = {
        .format = surface_format.format,
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
    
    VkAttachmentDescription attachments[2] = { color_att, depth_att };
    VkRenderPassCreateInfo rp_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 2,
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };
    
    VkRenderPass render_pass;
    vkCreateRenderPass(device, &rp_info, nullptr, &render_pass);
    
    // Depth buffer
    VkImage depth_image;
    VkDeviceMemory depth_mem;
    VkImageView depth_view;
    {
        VkImageCreateInfo depth_img_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_D32_SFLOAT,
            .extent = { extent.width, extent.height, 1 },
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
    }
    
    // Framebuffers
    std::vector<VkFramebuffer> framebuffers(image_count);
    for (uint32_t i = 0; i < image_count; i++) {
        VkImageView fb_attachments[2] = { sw_image_views[i], depth_view };
        VkFramebufferCreateInfo fb_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass,
            .attachmentCount = 2,
            .pAttachments = fb_attachments,
            .width = extent.width,
            .height = extent.height,
            .layers = 1,
        };
        vkCreateFramebuffer(device, &fb_info, nullptr, &framebuffers[i]);
    }
    
    // Command pool
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphics_family,
    };
    VkCommandPool cmd_pool;
    vkCreateCommandPool(device, &pool_info, nullptr, &cmd_pool);
    
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer cmd_buf;
    vkAllocateCommandBuffers(device, &alloc_info, &cmd_buf);
    
    // Camera UBO
    VkBuffer camera_ubo;
    VkDeviceMemory camera_ubo_mem;
    {
        VkBufferCreateInfo ubo_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = 128,
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        vkCreateBuffer(device, &ubo_info, nullptr, &camera_ubo);
        
        VkMemoryRequirements ubo_mem_req;
        vkGetBufferMemoryRequirements(device, camera_ubo, &ubo_mem_req);
        
        uint32_t ubo_mem_type = find_host_mem_type(physical, ubo_mem_req.memoryTypeBits);
        VkMemoryAllocateInfo ubo_alloc = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = ubo_mem_req.size,
            .memoryTypeIndex = ubo_mem_type,
        };
        vkAllocateMemory(device, &ubo_alloc, nullptr, &camera_ubo_mem);
        vkBindBufferMemory(device, camera_ubo, camera_ubo_mem, 0);
    }
    
    // Descriptor set
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
    
    VkDescriptorSetAllocateInfo ds_alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = desc_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &desc_set_layout,
    };
    
    VkDescriptorSet desc_set;
    vkAllocateDescriptorSets(device, &ds_alloc, &desc_set);
    
    VkDescriptorBufferInfo ubo_buf_info = {
        .buffer = camera_ubo,
        .offset = 0,
        .range = 128,
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
    
    // Building vertex and index buffers
    VkBuffer building_vb, building_ib;
    VkDeviceMemory building_mem;
    {
        VkDeviceSize vb_size = buildingBatch.vertices.size() * sizeof(bldg::BuildingVertex);
        VkDeviceSize ib_size = buildingBatch.indices.size() * sizeof(uint32_t);
        
        VkBufferCreateInfo vb_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = vb_size,
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        vkCreateBuffer(device, &vb_info, nullptr, &building_vb);
        
        VkBufferCreateInfo ib_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = ib_size,
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        vkCreateBuffer(device, &ib_info, nullptr, &building_ib);
        
        VkMemoryRequirements vb_mem_req, ib_mem_req;
        vkGetBufferMemoryRequirements(device, building_vb, &vb_mem_req);
        vkGetBufferMemoryRequirements(device, building_ib, &ib_mem_req);
        
        VkDeviceSize ib_offset = (vb_mem_req.size + vb_mem_req.alignment - 1) & ~(vb_mem_req.alignment - 1);
        VkDeviceSize total_size = ib_offset + ib_mem_req.size;
        
        uint32_t mem_type = find_host_mem_type(physical, vb_mem_req.memoryTypeBits);
        VkMemoryAllocateInfo mem_alloc = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = total_size,
            .memoryTypeIndex = mem_type,
        };
        vkAllocateMemory(device, &mem_alloc, nullptr, &building_mem);
        vkBindBufferMemory(device, building_vb, building_mem, 0);
        vkBindBufferMemory(device, building_ib, building_mem, ib_offset);
        
        void* mapped;
        vkMapMemory(device, building_mem, 0, vb_size, 0, &mapped);
        memcpy(mapped, buildingBatch.vertices.data(), vb_size);
        vkUnmapMemory(device, building_mem);
        
        vkMapMemory(device, building_mem, ib_offset, ib_size, 0, &mapped);
        memcpy(mapped, buildingBatch.indices.data(), ib_size);
        vkUnmapMemory(device, building_mem);
    }
    printf("Building buffers created\n");
    
    // Load building shaders
    auto vert_code = load_spv("src/shaders/building.vert.spv");
    auto frag_code = load_spv("src/shaders/building.frag.spv");
    if (vert_code.empty() || frag_code.empty()) {
        fprintf(stderr, "Building shaders not found\n");
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
    
    // Pipeline layout
    VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = 4 * sizeof(float),
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
    
    // Vertex input
    VkVertexInputBindingDescription binding = {
        .binding = 0,
        .stride = sizeof(bldg::BuildingVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkVertexInputAttributeDescription attrs[2] = {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(bldg::BuildingVertex, x),
        },
        {
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(bldg::BuildingVertex, nx),
        }
    };
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = attrs,
    };
    
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };
    
    VkViewport viewport = { 0, 0, (float)extent.width, (float)extent.height, 0.0f, 1.0f };
    VkRect2D scissor = { {0, 0}, extent };
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
    VkResult res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "Failed to create pipeline: %d\n", res);
        return 1;
    }
    printf("Pipeline created\n");
    
    // Update camera matrices
    float center_x = (min_x + max_x) * 0.5f;
    float center_z = (min_z + max_z) * 0.5f;
    float range_x = (max_x - min_x) * 0.5f;
    float range_z = (max_z - min_z) * 0.5f;
    float max_range = std::max(range_x, range_z);
    float max_height = max_y; // Maximum building height
    float padding = 1.5f;
    float tilt_angle = 45.0f;
    float tilt_rad = glm::radians(tilt_angle);
    
    // Camera must be higher than tallest building
    float cam_distance = std::max(max_range * 2.0f * padding, max_height * 1.5f);
    float cam_height = cam_distance * std::cos(tilt_rad);
    float cam_offset = cam_distance * std::sin(tilt_rad);
    
    printf("Camera setup: center=(%.0f, %.0f), range=%.0f, max_height=%.0f\n", 
           center_x, center_z, max_range, max_height);
    printf("  cam_distance=%.0f, cam_height=%.0f, cam_offset=%.0f\n", 
           cam_distance, cam_height, cam_offset);
    
    float aspect = (float)extent.width / (float)extent.height;
    float far_plane = std::max(1000.0f, max_height * 10.0f);
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, far_plane);
    
    glm::vec3 cam_pos(center_x, cam_height, center_z + cam_offset);
    glm::vec3 look_at(center_x, 0.0f, center_z);
    glm::mat4 view = glm::lookAt(cam_pos, look_at, glm::vec3(0.0f, 1.0f, 0.0f));
    
    void* ubo_mapped;
    vkMapMemory(device, camera_ubo_mem, 0, 128, 0, &ubo_mapped);
    memcpy(ubo_mapped, &proj[0][0], 64);
    memcpy(static_cast<char*>(ubo_mapped) + 64, &view[0][0], 64);
    vkUnmapMemory(device, camera_ubo_mem);
    printf("Camera matrices uploaded\n");
    
    // Record command buffer
    VkCommandBufferBeginInfo begin_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(cmd_buf, &begin_info);
    
    VkClearValue clear[2] = {
        { .color = { {0.1f, 0.1f, 0.2f, 1.0f} } },
        { .depthStencil = { 1.0f, 0 } }
    };
    VkRenderPassBeginInfo rp_begin = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = render_pass,
        .framebuffer = framebuffers[0],
        .renderArea = { {0, 0}, extent },
        .clearValueCount = 2,
        .pClearValues = clear,
    };
    vkCmdBeginRenderPass(cmd_buf, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
    
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &desc_set, 0, nullptr);
    
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buf, 0, 1, &building_vb, offsets);
    vkCmdBindIndexBuffer(cmd_buf, building_ib, 0, VK_INDEX_TYPE_UINT32);
    
    // Push constant - building color (light gray)
    float color[4] = { 0.7f, 0.7f, 0.7f, 1.0f };
    vkCmdPushConstants(cmd_buf, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(color), color);
    
    vkCmdDrawIndexed(cmd_buf, static_cast<uint32_t>(buildingBatch.indices.size()), 1, 0, 0, 0);
    
    vkCmdEndRenderPass(cmd_buf);
    vkEndCommandBuffer(cmd_buf);
    printf("Command buffer recorded\n");
    
    // Submit and present
    VkFenceCreateInfo fence_info = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };
    VkFence fence;
    vkCreateFence(device, &fence_info, nullptr, &fence);
    
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd_buf,
    };
    vkQueueSubmit(graphics_queue, 1, &submit_info, fence);
    vkWaitForFences(device, 1, &fence, VK_TRUE, 5000000000);
    
    uint32_t image_idx;
    vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, VK_NULL_HANDLE, VK_NULL_HANDLE, &image_idx);
    
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 0,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &image_idx,
    };
    vkQueuePresentKHR(graphics_queue, &present_info);
    
    printf("Buildings should be visible! Press ESC to quit.\n");
    
    bool running = true;
    SDL_Event ev;
    while (running) {
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT || (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE)) {
                running = false;
            }
        }
        SDL_Delay(10);
    }
    
    vkDeviceWaitIdle(device);
    
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    vkDestroyDescriptorPool(device, desc_pool, nullptr);
    vkDestroyDescriptorSetLayout(device, desc_set_layout, nullptr);
    vkDestroyBuffer(device, camera_ubo, nullptr);
    vkFreeMemory(device, camera_ubo_mem, nullptr);
    vkDestroyBuffer(device, building_vb, nullptr);
    vkDestroyBuffer(device, building_ib, nullptr);
    vkFreeMemory(device, building_mem, nullptr);
    vkDestroyShaderModule(device, frag_mod, nullptr);
    vkDestroyShaderModule(device, vert_mod, nullptr);
    vkDestroyCommandPool(device, cmd_pool, nullptr);
    vkDestroyImageView(device, depth_view, nullptr);
    vkDestroyImage(device, depth_image, nullptr);
    vkFreeMemory(device, depth_mem, nullptr);
    vkDestroyRenderPass(device, render_pass, nullptr);
    for (auto& v : sw_image_views) vkDestroyImageView(device, v, nullptr);
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    printf("Clean exit\n");
    return 0;
}

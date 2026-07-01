// test_triangle.cpp — Minimal Vulkan triangle test
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <cstdio>
#include <vector>
#include <fstream>
#include <cstring>
#include <glm/glm.hpp>

constexpr int WIDTH = 800;
constexpr int HEIGHT = 600;

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
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    printf("SDL initialized\n");

    SDL_Window* window = SDL_CreateWindow(
        "Triangle Test",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIDTH, HEIGHT,
        SDL_WINDOW_VULKAN
    );
    if (!window) {
        fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    printf("Window created successfully\n");

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Triangle Test",
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
    VkResult res = vkCreateInstance(&instance_info, nullptr, &instance);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan instance: %d\n", res);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("Vulkan instance created\n");

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
    if (device_count == 0) {
        fprintf(stderr, "No physical devices found\n");
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    VkPhysicalDevice physical = devices[0];

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physical, &props);
    printf("GPU: %s\n", props.deviceName);

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
    res = vkCreateDevice(physical, &device_info, nullptr, &device);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "Failed to create device: %d\n", res);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("Device created\n");

    VkQueue graphics_queue;
    vkGetDeviceQueue(device, graphics_family, 0, &graphics_queue);

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
    res = vkCreateSwapchainKHR(device, &sw_info, nullptr, &swapchain);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "Failed to create swapchain: %d\n", res);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("Swapchain created\n");

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

    VkAttachmentReference color_ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
    };

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo rp_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_att,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    VkRenderPass render_pass;
    vkCreateRenderPass(device, &rp_info, nullptr, &render_pass);

    std::vector<VkFramebuffer> framebuffers(image_count);
    for (uint32_t i = 0; i < image_count; i++) {
        VkImageView attachments[1] = { sw_image_views[i] };
        VkFramebufferCreateInfo fb_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass,
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = extent.width,
            .height = extent.height,
            .layers = 1,
        };
        vkCreateFramebuffer(device, &fb_info, nullptr, &framebuffers[i]);
    }

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

    // Triangle vertex data
    struct Vertex {
        float pos[3];
        float color[3];
    };
    std::vector<Vertex> vertices = {
        {{0.0f,  0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    };

    VkBuffer vb;
    VkDeviceMemory vb_mem;
    {
        VkBufferCreateInfo vb_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = vertices.size() * sizeof(Vertex),
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        };
        vkCreateBuffer(device, &vb_info, nullptr, &vb);

        VkMemoryRequirements mem_req;
        vkGetBufferMemoryRequirements(device, vb, &mem_req);

        uint32_t mem_type = find_host_mem_type(physical, mem_req.memoryTypeBits);
        VkMemoryAllocateInfo mem_alloc = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = mem_req.size,
            .memoryTypeIndex = mem_type,
        };
        vkAllocateMemory(device, &mem_alloc, nullptr, &vb_mem);
        vkBindBufferMemory(device, vb, vb_mem, 0);

        void* mapped;
        vkMapMemory(device, vb_mem, 0, vertices.size() * sizeof(Vertex), 0, &mapped);
        memcpy(mapped, vertices.data(), vertices.size() * sizeof(Vertex));
        vkUnmapMemory(device, vb_mem);
    }

    // Load triangle shaders
    auto vert_code = load_spv("src/shaders/triangle.vert.spv");
    auto frag_code = load_spv("src/shaders/triangle.frag.spv");
    if (vert_code.empty() || frag_code.empty()) {
        fprintf(stderr, "Triangle shaders not found\n");
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("Shaders loaded: vert=%zu words, frag=%zu words\n", vert_code.size(), frag_code.size());

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

    VkVertexInputBindingDescription binding = {
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkVertexInputAttributeDescription pos_attr = {
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex, pos),
    };
    VkVertexInputAttributeDescription color_attr = {
        .location = 1,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex, color),
    };
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = &pos_attr,
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
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
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
    };

    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
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

    VkPipelineLayoutCreateInfo layout_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
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
        .pColorBlendState = &color_blend,
        .layout = pipeline_layout,
        .renderPass = render_pass,
        .subpass = 0,
    };

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create pipeline\n");
        return 1;
    }

    printf("Pipeline created. Drawing triangle...\n");

    VkCommandBufferBeginInfo begin_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(cmd_buf, &begin_info);

    VkClearValue clear = { .color = { {0.1f, 0.1f, 0.1f, 1.0f} } };
    VkRenderPassBeginInfo rp_begin = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = render_pass,
        .framebuffer = framebuffers[0],
        .renderArea = { {0, 0}, extent },
        .clearValueCount = 1,
        .pClearValues = &clear,
    };
    vkCmdBeginRenderPass(cmd_buf, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buf, 0, 1, &vb, offsets);
    vkCmdDraw(cmd_buf, 3, 1, 0, 0);

    vkCmdEndRenderPass(cmd_buf);
    vkEndCommandBuffer(cmd_buf);

    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
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

    printf("Triangle should be visible! Press ESC to quit.\n");

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
    vkDestroyRenderPass(device, render_pass, nullptr);
    for (auto& fb : framebuffers) vkDestroyFramebuffer(device, fb, nullptr);
    vkDestroyCommandPool(device, cmd_pool, nullptr);
    for (auto& v : sw_image_views) vkDestroyImageView(device, v, nullptr);
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    vkDestroyBuffer(device, vb, nullptr);
    vkFreeMemory(device, vb_mem, nullptr);
    vkDestroyShaderModule(device, frag_mod, nullptr);
    vkDestroyShaderModule(device, vert_mod, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("Clean exit.\n");
    return 0;
}

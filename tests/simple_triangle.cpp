// simple_triangle.cpp — Minimal triangle test
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <cstdio>
#include <vector>
#include <fstream>

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

int main() {
    fprintf(stderr, "MAIN START\n");
    fflush(stderr);
    
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    printf("SDL_Init OK\n");
    
    SDL_Window* window = SDL_CreateWindow(
        "Simple Triangle",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_VULKAN
    );
    if (!window) {
        fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    printf("Window OK\n");
    
    printf("Creating Vulkan instance...\n");
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Simple Triangle",
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
        fprintf(stderr, "vkCreateInstance failed: %d\n", res);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("Instance OK\n");
    
    printf("Creating surface...\n");
    VkSurfaceKHR surface;
    SDL_bool surface_res = SDL_Vulkan_CreateSurface(window, instance, &surface);
    if (!surface_res) {
        fprintf(stderr, "Failed to create surface\n");
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("Surface OK\n");
    
    printf("Enumerating physical devices...\n");
    uint32_t device_count;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
    if (device_count == 0) {
        fprintf(stderr, "No physical devices\n");
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    VkPhysicalDevice physical = devices[0];
    printf("Found %u devices\n", device_count);
    
    printf("Creating device...\n");
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
    VkResult device_res = vkCreateDevice(physical, &device_info, nullptr, &device);
    if (device_res != VK_SUCCESS) {
        fprintf(stderr, "vkCreateDevice failed: %d\n", device_res);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("Device OK\n");
    
    printf("Creating swapchain...\n");
    VkSurfaceFormatKHR surface_format;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, formats.data());
    surface_format = formats[0];
    
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    VkExtent2D extent = { 800, 600 };
    
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
    VkResult sw_res = vkCreateSwapchainKHR(device, &sw_info, nullptr, &swapchain);
    if (sw_res != VK_SUCCESS) {
        fprintf(stderr, "vkCreateSwapchainKHR failed: %d\n", sw_res);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("Swapchain OK\n");
    
    printf("Creating image views...\n");
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
    printf("Image views OK\n");
    
    printf("Creating render pass...\n");
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
    VkResult rp_res = vkCreateRenderPass(device, &rp_info, nullptr, &render_pass);
    if (rp_res != VK_SUCCESS) {
        fprintf(stderr, "vkCreateRenderPass failed: %d\n", rp_res);
        return 1;
    }
    printf("Render pass OK\n");
    
    printf("Creating framebuffers...\n");
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
    printf("Framebuffers OK\n");
    
    printf("Creating command pool...\n");
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
    printf("Command pool OK\n");
    
    printf("Loading shaders...\n");
    auto vert_code = load_spv("src/shaders/triangle.vert.spv");
    auto frag_code = load_spv("src/shaders/triangle.frag.spv");
    if (vert_code.empty() || frag_code.empty()) {
        fprintf(stderr, "Shaders not found\n");
        return 1;
    }
    printf("  Shaders loaded: vert=%zu, frag=%zu\n", vert_code.size(), frag_code.size());
    
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
    printf("Shaders OK\n");
    
    return 0; // Skip pipeline creation for now - shaders use UBO and push constants
    
    printf("Creating pipeline layout...\n");
    printf("Creating graphics pipeline...\n");
    
    SDL_Delay(1000);
    
    vkDestroyShaderModule(device, frag_mod, nullptr);
    vkDestroyShaderModule(device, vert_mod, nullptr);
    vkDestroyCommandPool(device, cmd_pool, nullptr);
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

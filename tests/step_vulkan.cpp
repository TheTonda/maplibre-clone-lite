// step_vulkan.cpp — Step by step Vulkan test
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <cstdio>
#include <vector>

int main() {
    printf("Step 1: SDL_Init\n");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    
    printf("Step 2: SDL_CreateWindow\n");
    SDL_Window* window = SDL_CreateWindow(
        "Step Test",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_VULKAN
    );
    if (!window) {
        fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    
    printf("Step 3: vkCreateInstance\n");
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Step Test",
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
    
    printf("Step 4: SDL_Vulkan_CreateSurface\n");
    VkSurfaceKHR surface;
    SDL_bool surface_res = SDL_Vulkan_CreateSurface(window, instance, &surface);
    if (!surface_res) {
        fprintf(stderr, "Failed to create surface\n");
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    printf("Step 5: vkEnumeratePhysicalDevices\n");
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
    
    printf("Step 6: vkCreateDevice\n");
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
        fprintf(stderr, "vkCreateDevice failed: %d\n", res);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    printf("Step 7: vkCreateSwapchainKHR\n");
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
    res = vkCreateSwapchainKHR(device, &sw_info, nullptr, &swapchain);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "vkCreateSwapchainKHR failed: %d\n", res);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    printf("All steps completed successfully!\n");
    
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}

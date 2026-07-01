// minimal_window.cpp — Just test window creation
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <cstdio>
#include <vector>

int main() {
    fprintf(stderr, "MAIN START\n");
    fflush(stderr);
    printf("Step 1: SDL_Init\n");
    fflush(stdout);
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    printf("  SDL_Init succeeded\n");
    fflush(stdout);
    
    printf("Step 2: SDL_CreateWindow\n");
    fflush(stdout);
    SDL_Window* window = SDL_CreateWindow(
        "Test",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_VULKAN
    );
    if (!window) {
        fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    printf("  Window created\n");
    fflush(stdout);
    
    printf("Step 3: vkCreateInstance\n");
    fflush(stdout);
    
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Test",
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
    printf("  Instance created\n");
    fflush(stdout);
    
    printf("Step 4: SDL_Vulkan_CreateSurface\n");
    fflush(stdout);
    
    VkSurfaceKHR surface;
    SDL_bool surface_res = SDL_Vulkan_CreateSurface(window, instance, &surface);
    if (!surface_res) {
        fprintf(stderr, "Failed to create surface\n");
        vkDestroyInstance(instance, nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("  Surface created\n");
    fflush(stdout);
    
    printf("Step 5: vkEnumeratePhysicalDevices\n");
    fflush(stdout);
    
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
    printf("  Found %u devices\n", device_count);
    fflush(stdout);
    
    printf("Step 6: vkCreateDevice\n");
    fflush(stdout);
    
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
    printf("  Device created\n");
    fflush(stdout);
    
    SDL_Delay(1000);
    
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    printf("Clean exit\n");
    return 0;
}

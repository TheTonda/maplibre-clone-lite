// minimal_vulkan.cpp — Just test Vulkan instance creation
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <cstdio>
#include <vector>

int main() {
    printf("Starting minimal Vulkan test...\n");
    
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    printf("SDL initialized\n");
    
    SDL_Window* window = SDL_CreateWindow(
        "Minimal Vulkan Test",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_VULKAN
    );
    if (!window) {
        fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    printf("Window created\n");
    
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Test",
        .apiVersion = VK_API_VERSION_1_3,
    };
    
    unsigned ext_count;
    SDL_Vulkan_GetInstanceExtensions(window, &ext_count, nullptr);
    std::vector<const char*> extensions(ext_count);
    SDL_Vulkan_GetInstanceExtensions(window, &ext_count, extensions.data());
    
    printf("Extensions: %u\n", ext_count);
    for (unsigned i = 0; i < ext_count; i++) {
        printf("  %s\n", extensions[i]);
    }
    
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
    printf("Vulkan instance created\n");
    
    uint32_t device_count;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    printf("Physical devices: %u\n", device_count);
    
    if (device_count > 0) {
        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
        
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[0], &props);
        printf("GPU: %s\n", props.deviceName);
    }
    
    vkDestroyInstance(instance, nullptr);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    printf("Clean exit\n");
    return 0;
}

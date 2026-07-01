/// @file vulkan_context.cpp
/// @brief Vulkan instance, device, and swapchain management.

#include "core/vulkan_context.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

// -----------------------------------------------------------------------
// Validation-layer debug callback
// -----------------------------------------------------------------------

static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
               VkDebugUtilsMessageTypeFlagsEXT             /*type*/,
               const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
               void*                                       /*user_data*/)
{
    // Filter out verbose / info — only show warnings and errors
    if (severity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT))
    {
        std::fprintf(stderr, "[VULKAN] %s\n", callback_data->pMessage);
    }
    return VK_FALSE;
}

// -----------------------------------------------------------------------
// Construction / Destruction
// -----------------------------------------------------------------------

VulkanContext::VulkanContext(VulkanContext&& other) noexcept
    : instance_(other.instance_)
    , debug_messenger_(other.debug_messenger_)
    , physical_device_(other.physical_device_)
    , device_(other.device_)
    , graphics_queue_(other.graphics_queue_)
    , present_queue_(other.present_queue_)
    , graphics_family_(other.graphics_family_)
    , present_family_(other.present_family_)
    , surface_(other.surface_)
    , cleanup_performed_(other.cleanup_performed_)
{
    other.instance_          = VK_NULL_HANDLE;
    other.debug_messenger_   = VK_NULL_HANDLE;
    other.physical_device_   = VK_NULL_HANDLE;
    other.device_            = VK_NULL_HANDLE;
    other.surface_           = VK_NULL_HANDLE;
    other.cleanup_performed_ = true;
}

VulkanContext& VulkanContext::operator=(VulkanContext&& other) noexcept {
    if (this != &other) {
        cleanup();
        instance_          = other.instance_;
        debug_messenger_   = other.debug_messenger_;
        physical_device_   = other.physical_device_;
        device_            = other.device_;
        graphics_queue_    = other.graphics_queue_;
        present_queue_     = other.present_queue_;
        graphics_family_   = other.graphics_family_;
        present_family_    = other.present_family_;
        surface_           = other.surface_;
        cleanup_performed_ = other.cleanup_performed_;
        other.instance_          = VK_NULL_HANDLE;
        other.debug_messenger_   = VK_NULL_HANDLE;
        other.physical_device_   = VK_NULL_HANDLE;
        other.device_            = VK_NULL_HANDLE;
        other.surface_           = VK_NULL_HANDLE;
        other.cleanup_performed_ = true;
    }
    return *this;
}

// -----------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------

void VulkanContext::initialize(Window& window) {
    surface_ = window.get_surface();
    extent_  = {static_cast<uint32_t>(window.get_width()),
                static_cast<uint32_t>(window.get_height())};

    create_instance();
    setup_debug_messenger();
    pick_physical_device();
    create_logical_device();

    std::fprintf(stdout, "[INFO]  VulkanContext initialised.\n");
}

void VulkanContext::cleanup() {
    if (cleanup_performed_) return;
    cleanup_performed_ = true;

    // Destroy logical device (handles queues automatically)
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    // Destroy debug messenger
    if (debug_messenger_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
        if (func) {
            func(instance_, debug_messenger_, nullptr);
        }
        debug_messenger_ = VK_NULL_HANDLE;
    }

    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }

    std::fprintf(stdout, "[INFO]  VulkanContext cleaned up.\n");
}

// -----------------------------------------------------------------------
// Instance Creation
// -----------------------------------------------------------------------

void VulkanContext::create_instance() {
    // Check validation layers
    enable_validation_layers_ = check_validation_layer_support();
    if (!enable_validation_layers_) {
        std::fprintf(stderr, "[WARN]  Validation layers requested but not "
                             "available — continuing without them.\n");
    }

    // Application info
    VkApplicationInfo app_info{};
    app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName   = "Map Renderer";
    app_info.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    app_info.pEngineName        = "Map Renderer";
    app_info.engineVersion      = VK_MAKE_API_VERSION(0, 1, 0, 0);
    app_info.apiVersion         = VK_API_VERSION_1_2;

    // Instance create info
    VkInstanceCreateInfo create_info{};
    create_info.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    auto extensions = get_required_extensions();
    create_info.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();

    if (enable_validation_layers_) {
        create_info.enabledLayerCount   = static_cast<uint32_t>(validation_layers_.size());
        create_info.ppEnabledLayerNames = validation_layers_.data();
    } else {
        create_info.enabledLayerCount = 0;
    }

    VkResult res = vkCreateInstance(&create_info, nullptr, &instance_);
    if (res != VK_SUCCESS) {
        std::fprintf(stderr, "[ERROR] vkCreateInstance failed (%d)\n", res);
        std::abort();
    }
}

void VulkanContext::setup_debug_messenger() {
    if (!enable_validation_layers_) return;

    VkDebugUtilsMessengerCreateInfoEXT debug_info{};
    debug_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT    |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT  |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_info.pfnUserCallback = debug_callback;

    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
    if (!func) {
        std::fprintf(stderr, "[WARN]  vkCreateDebugUtilsMessengerEXT not found.\n");
        return;
    }

    VkResult res = func(instance_, &debug_info, nullptr, &debug_messenger_);
    if (res != VK_SUCCESS) {
        std::fprintf(stderr, "[WARN]  Failed to set up debug messenger (%d)\n", res);
    }
}

// -----------------------------------------------------------------------
// Physical Device Selection
// -----------------------------------------------------------------------

void VulkanContext::pick_physical_device() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (count == 0) {
        std::fprintf(stderr, "[ERROR] No Vulkan-capable physical devices found.\n");
        std::abort();
    }

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());

    // Prefer discrete GPU, then integrated, then fallback
    for (const auto& dev : devices) {
        if (is_device_suitable(dev)) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(dev, &props);
            std::fprintf(stdout, "[INFO]  Selected GPU: %s (type=%d)\n",
                         props.deviceName, props.deviceType);

            // Prefer discrete GPU
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                physical_device_ = dev;
                break;
            }
            if (physical_device_ == VK_NULL_HANDLE) {
                physical_device_ = dev;
            }
        }
    }

    if (physical_device_ == VK_NULL_HANDLE) {
        std::fprintf(stderr, "[ERROR] No suitable physical device found.\n");
        std::abort();
    }
}

bool VulkanContext::is_device_suitable(VkPhysicalDevice device) const {
    // Check queue families
    auto indices = find_queue_families(device, surface_);
    if (!indices.is_complete()) return false;

    // Check device extensions
    uint32_t ext_count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &ext_count, nullptr);
    std::vector<VkExtensionProperties> available_exts(ext_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &ext_count,
                                         available_exts.data());

    bool has_swapchain = false;
    const char* required = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    for (const auto& ext : available_exts) {
        if (std::strcmp(ext.extensionName, required) == 0) {
            has_swapchain = true;
            break;
        }
    }
    return has_swapchain;
}

VulkanContext::QueueFamilyIndices
VulkanContext::find_queue_families(VkPhysicalDevice device,
                                   VkSurfaceKHR surface) const
{
    QueueFamilyIndices indices;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (uint32_t i = 0; i < count; ++i) {
        // Graphics queue
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics = i;
        }
        // Present queue
        VkBool32 present_support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface,
                                             &present_support);
        if (present_support) {
            indices.present = i;
        }
        if (indices.is_complete()) break;
    }
    return indices;
}

// -----------------------------------------------------------------------
// Logical Device
// -----------------------------------------------------------------------

void VulkanContext::create_logical_device() {
    auto indices = find_queue_families(physical_device_, surface_);
    graphics_family_ = indices.graphics.value();
    present_family_  = indices.present.value();

    // Unique queue families
    std::set<uint32_t> unique_families = {graphics_family_, present_family_};

    float queue_priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queue_infos;
    for (uint32_t family : unique_families) {
        VkDeviceQueueCreateInfo qci{};
        qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = family;
        qci.queueCount       = 1;
        qci.pQueuePriorities = &queue_priority;
        queue_infos.push_back(qci);
    }

    // Enable swapchain extension
    const char* device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo create_info{};
    create_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount    = static_cast<uint32_t>(queue_infos.size());
    create_info.pQueueCreateInfos       = queue_infos.data();
    create_info.enabledExtensionCount   = 1;
    create_info.ppEnabledExtensionNames = device_extensions;

    // (Optional) enable validation layers on the device as well
    if (enable_validation_layers_) {
        create_info.enabledLayerCount   = static_cast<uint32_t>(validation_layers_.size());
        create_info.ppEnabledLayerNames = validation_layers_.data();
    }

    VkResult res = vkCreateDevice(physical_device_, &create_info,
                                  nullptr, &device_);
    if (res != VK_SUCCESS) {
        std::fprintf(stderr, "[ERROR] vkCreateDevice failed (%d)\n", res);
        std::abort();
    }

    vkGetDeviceQueue(device_, graphics_family_, 0, &graphics_queue_);
    vkGetDeviceQueue(device_, present_family_,  0, &present_queue_);
}

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

bool VulkanContext::check_validation_layer_support() const {
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> available(count);
    vkEnumerateInstanceLayerProperties(&count, available.data());

    for (const char* layer : validation_layers_) {
        bool found = false;
        for (const auto& prop : available) {
            if (std::strcmp(layer, prop.layerName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

std::vector<const char*>
VulkanContext::get_required_extensions() const {
    uint32_t count = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(nullptr, &count, nullptr)) {
        std::fprintf(stderr, "[WARN]  SDL_Vulkan_GetInstanceExtensions count failed.\n");
        return {};
    }

    std::vector<const char*> exts(count);
    if (!SDL_Vulkan_GetInstanceExtensions(nullptr, &count, exts.data())) {
        std::fprintf(stderr, "[WARN]  SDL_Vulkan_GetInstanceExtensions failed.\n");
        return {};
    }

    if (enable_validation_layers_) {
        exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return exts;
}

VulkanContext::SwapchainSupportDetails
VulkanContext::query_swapchain_support(VkPhysicalDevice device,
                                       VkSurfaceKHR surface)
{
    SwapchainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
                                              &details.capabilities);

    uint32_t fmt_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &fmt_count, nullptr);
    if (fmt_count > 0) {
        details.formats.resize(fmt_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &fmt_count,
                                             details.formats.data());
    }

    uint32_t mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &mode_count,
                                              nullptr);
    if (mode_count > 0) {
        details.present_modes.resize(mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &mode_count,
                                                  details.present_modes.data());
    }
    return details;
}

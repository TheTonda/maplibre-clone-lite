#pragma once

/// @file vulkan_context.h
/// @brief Manages the Vulkan instance, physical/logical device, and
///        swapchain-related resources.

#include <vulkan/vulkan.h>
#include <vector>
#include <optional>

#include "core/window.h"

class VulkanContext {
public:
    VulkanContext() = default;
    ~VulkanContext() { cleanup(); }

    // Non-copyable
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    // Allow move (common pattern for RAII Vulkan wrappers)
    VulkanContext(VulkanContext&& other) noexcept;
    VulkanContext& operator=(VulkanContext&& other) noexcept;

    /// Initialise the instance, pick a physical device, and create the
    /// logical device.  The caller must provide a valid Window so we can
    /// obtain the Vulkan surface.
    void initialize(Window& window);

    /// Destroy all Vulkan resources in reverse order.
    void cleanup();

    // ---- Getters ----
    VkInstance       get_instance()        const { return instance_; }
    VkPhysicalDevice get_physical_device() const { return physical_device_; }
    VkDevice         get_device()          const { return device_; }
    VkQueue          get_graphics_queue()  const { return graphics_queue_; }
    VkQueue          get_present_queue()   const { return present_queue_; }
    uint32_t         get_graphics_family() const { return graphics_family_; }
    uint32_t         get_present_family()  const { return present_family_; }
    VkSurfaceKHR     get_surface()         const { return surface_; }
    VkExtent2D       get_extent()          const { return extent_; }

    /// Queue family indices for a given physical device.
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphics;
        std::optional<uint32_t> present;

        bool is_complete() const {
            return graphics.has_value() && present.has_value();
        }
    };

    /// Swapchain support details queried from a physical device.
    struct SwapchainSupportDetails {
        VkSurfaceCapabilitiesKHR        capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR>   present_modes;
    };

    /// Query swapchain support for a given physical device + surface.
    static SwapchainSupportDetails
    query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface);

private:
    void create_instance();
    void setup_debug_messenger();
    void pick_physical_device();
    void create_logical_device();

    bool check_validation_layer_support() const;

    std::vector<const char*> get_required_extensions() const;

    QueueFamilyIndices
    find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface) const;

    bool is_device_suitable(VkPhysicalDevice device) const;

    // ---- State ----
    VkInstance       instance_          = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_   = VK_NULL_HANDLE;
    VkDevice         device_            = VK_NULL_HANDLE;
    VkQueue          graphics_queue_    = VK_NULL_HANDLE;
    VkQueue          present_queue_     = VK_NULL_HANDLE;
    uint32_t         graphics_family_   = 0;
    uint32_t         present_family_    = 0;
    VkSurfaceKHR     surface_           = VK_NULL_HANDLE;
    VkExtent2D       extent_            = {};
    bool             cleanup_performed_ = false;

    const std::vector<const char*> validation_layers_ = {
        "VK_LAYER_KHRONOS_validation"
    };
    bool enable_validation_layers_ = true;
};

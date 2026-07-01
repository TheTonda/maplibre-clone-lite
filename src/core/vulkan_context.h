#pragma once

/// @file vulkan_context.h
/// @brief Manages the Vulkan instance, device, swapchain, render pass,
///        framebuffers, and command pool.

#include <vulkan/vulkan.h>
#include <vector>
#include <optional>

#include "core/window.h"

/// High-level container for all Vulkan resources owned by the application.
class VulkanContext {
public:
    VulkanContext() = default;
    ~VulkanContext() { cleanup(); }

    // Non-copyable
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    // Move support
    VulkanContext(VulkanContext&& other) noexcept;
    VulkanContext& operator=(VulkanContext&& other) noexcept;

    /// Initialise the full Vulkan pipeline up to the framebuffers.
    /// Must be called after the Window is constructed.
    void initialize(Window& window);

    /// Destroy all Vulkan resources in reverse creation order.
    void cleanup();

    // ---- Getters ----
    VkInstance              get_instance()            const { return instance_; }
    VkPhysicalDevice        get_physical_device()     const { return physical_device_; }
    VkDevice                get_device()              const { return device_; }
    VkQueue                 get_graphics_queue()      const { return graphics_queue_; }
    VkQueue                 get_present_queue()       const { return present_queue_; }
    uint32_t                get_graphics_family()     const { return graphics_family_; }
    uint32_t                get_present_family()      const { return present_family_; }
    VkSurfaceKHR            get_surface()             const { return surface_; }
    VkExtent2D              get_extent()              const { return extent_; }
    VkRenderPass            get_render_pass()         const { return render_pass_; }
    VkCommandPool           get_command_pool()        const { return command_pool_; }
    VkFormat                get_depth_format()        const { return depth_format_; }

    /// Number of swapchain images.
    size_t get_swapchain_image_count() const { return swapchain_images_.size(); }

    /// Current frame-in-flight index (0 or 1).
    uint32_t get_current_frame() const { return current_frame_; }

    /// Allocate a command buffer from the pool.
    VkCommandBuffer allocate_command_buffer() const;

    /// Begin a one-time-submit command buffer.
    VkCommandBuffer begin_one_time_commands() const;

    /// End and submit a one-time-submit command buffer.
    void end_one_time_commands(VkCommandBuffer cmd) const;

    /// Acquire the next swapchain image index.
    /// @return The image index, or the special value ~0u on suboptimal/out-of-date.
    uint32_t acquire_next_image();

    /// Record and submit a draw command buffer for the current framebuffer,
    /// then present the result.  The optional callback lets a Renderer inject
    /// draw commands between cmdBeginRenderPass / cmdEndRenderPass.
    void submit_frame(uint32_t image_index,
                       void (*record_fn)(VkCommandBuffer, void*) = nullptr,
                       void* user_data = nullptr);

    /// Swapchain framebuffer at index @p i.
    VkFramebuffer           get_framebuffer(size_t i) const { return framebuffers_[i]; }

    /// Swapchain image at index @p i.
    VkImageView             get_image_view(size_t i)  const { return swapchain_image_views_[i]; }

    /// Queue family indices for a physical device.
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphics;
        std::optional<uint32_t> present;

        bool is_complete() const {
            return graphics.has_value() && present.has_value();
        }
    };

    /// Capabilities, formats, and present modes for a surface.
    struct SwapchainSupportDetails {
        VkSurfaceCapabilitiesKHR        capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR>   present_modes;
    };

    /// Query swapchain support for a given device + surface.
    static SwapchainSupportDetails
    query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface);

private:
    // Initialisation steps (called in order by initialize())
    void create_instance();
    void setup_debug_messenger();
    void pick_physical_device();
    void create_logical_device();
    void create_swapchain();
    void create_image_views();
    void create_depth_resources();
    void create_render_pass();
    void create_framebuffers();
    void create_command_pool();
    void create_sync_objects();

    // Swapchain helpers
    VkSurfaceFormatKHR choose_swapchain_format(
        const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR   choose_present_mode(
        const std::vector<VkPresentModeKHR>& modes) const;
    VkExtent2D         choose_extent(
        const VkSurfaceCapabilitiesKHR& caps) const;

    // Validation helpers
    bool check_validation_layer_support() const;
    std::vector<const char*> get_required_extensions() const;
    QueueFamilyIndices find_queue_families(VkPhysicalDevice device,
                                           VkSurfaceKHR surface) const;
    bool is_device_suitable(VkPhysicalDevice device) const;

    // ---- State ----
    // Instance / device
    VkInstance              instance_          = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger_  = VK_NULL_HANDLE;
    VkPhysicalDevice        physical_device_   = VK_NULL_HANDLE;
    VkDevice                device_            = VK_NULL_HANDLE;

    // Queues
    VkQueue                 graphics_queue_    = VK_NULL_HANDLE;
    VkQueue                 present_queue_     = VK_NULL_HANDLE;
    uint32_t                graphics_family_   = 0;
    uint32_t                present_family_    = 0;

    // Surface / swapchain
    VkSurfaceKHR            surface_           = VK_NULL_HANDLE;
    VkSwapchainKHR          swapchain_         = VK_NULL_HANDLE;
    std::vector<VkImage>    swapchain_images_;
    std::vector<VkImageView> swapchain_image_views_;
    VkFormat                swapchain_format_  = VK_FORMAT_UNDEFINED;
    VkExtent2D              extent_            = {};

    // Depth resources
    VkImage                 depth_image_       = VK_NULL_HANDLE;
    VkDeviceMemory          depth_image_memory_= VK_NULL_HANDLE;
    VkImageView             depth_image_view_  = VK_NULL_HANDLE;
    VkFormat                depth_format_      = VK_FORMAT_UNDEFINED;

    // Render pass
    VkRenderPass            render_pass_       = VK_NULL_HANDLE;

    // Framebuffers
    std::vector<VkFramebuffer> framebuffers_;

    // Command pool
    VkCommandPool           command_pool_      = VK_NULL_HANDLE;

    // Per-frame sync objects (double-buffered)
    static constexpr uint32_t kMaxFramesInFlight = 2;
    std::vector<VkSemaphore> image_available_;
    std::vector<VkSemaphore> render_finished_;
    std::vector<VkFence>     in_flight_fences_;
    uint32_t                 current_frame_    = 0;

    bool                    cleanup_performed_ = false;

    // Validation
    const std::vector<const char*> validation_layers_ = {
        "VK_LAYER_KHRONOS_validation"
    };
    bool enable_validation_layers_ = true;
};

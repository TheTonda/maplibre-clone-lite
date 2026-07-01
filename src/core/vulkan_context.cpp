/// @file vulkan_context.cpp
/// @brief Vulkan instance, device, swapchain, render pass, framebuffers,
///        and command pool implementation.

#include "core/vulkan_context.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
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
    if (severity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT))
    {
        std::fprintf(stderr, "[VULKAN] %s\n", callback_data->pMessage);
    }
    return VK_FALSE;
}

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

/// Find a suitable memory type from the physical device.
static uint32_t find_memory_type(VkPhysicalDevice phys_dev,
                                 uint32_t type_filter,
                                 VkMemoryPropertyFlags props)
{
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(phys_dev, &mem_props);

    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1u << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & props) == props)
        {
            return i;
        }
    }

    std::fprintf(stderr, "[ERROR] Failed to find suitable memory type.\n");
    std::abort();
}

/// Create an image and its device memory + view.
static void create_image(VkDevice device, VkPhysicalDevice phys_dev,
                         uint32_t width, uint32_t height,
                         VkFormat format, VkImageTiling tiling,
                         VkImageUsageFlags usage,
                         VkMemoryPropertyFlags props,
                         VkImage& image, VkDeviceMemory& image_memory)
{
    VkImageCreateInfo info{};
    info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType     = VK_IMAGE_TYPE_2D;
    info.extent.width  = width;
    info.extent.height = height;
    info.extent.depth  = 1;
    info.mipLevels     = 1;
    info.arrayLayers   = 1;
    info.format        = format;
    info.tiling        = tiling;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    info.usage         = usage;
    info.samples       = VK_SAMPLE_COUNT_1_BIT;
    info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &info, nullptr, &image) != VK_SUCCESS) {
        std::fprintf(stderr, "[ERROR] Failed to create image.\n");
        std::abort();
    }

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(device, image, &req);

    VkMemoryAllocateInfo alloc{};
    alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize  = req.size;
    alloc.memoryTypeIndex = find_memory_type(phys_dev, req.memoryTypeBits, props);

    if (vkAllocateMemory(device, &alloc, nullptr, &image_memory) != VK_SUCCESS) {
        std::fprintf(stderr, "[ERROR] Failed to allocate image memory.\n");
        std::abort();
    }

    vkBindImageMemory(device, image, image_memory, 0);
}

static VkImageView create_image_view(VkDevice device, VkImage image,
                                     VkFormat format, VkImageAspectFlags aspect)
{
    VkImageViewCreateInfo info{};
    info.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.image    = image;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format   = format;
    info.subresourceRange.aspectMask     = aspect;
    info.subresourceRange.baseMipLevel   = 0;
    info.subresourceRange.levelCount     = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount     = 1;

    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(device, &info, nullptr, &view) != VK_SUCCESS) {
        std::fprintf(stderr, "[ERROR] Failed to create image view.\n");
        std::abort();
    }
    return view;
}

// -----------------------------------------------------------------------
// Move semantics
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
    , swapchain_(other.swapchain_)
    , swapchain_images_(std::move(other.swapchain_images_))
    , swapchain_image_views_(std::move(other.swapchain_image_views_))
    , swapchain_format_(other.swapchain_format_)
    , extent_(other.extent_)
    , depth_image_(other.depth_image_)
    , depth_image_memory_(other.depth_image_memory_)
    , depth_image_view_(other.depth_image_view_)
    , depth_format_(other.depth_format_)
    , render_pass_(other.render_pass_)
    , framebuffers_(std::move(other.framebuffers_))
    , command_pool_(other.command_pool_)
    , image_available_(std::move(other.image_available_))
    , render_finished_(std::move(other.render_finished_))
    , in_flight_fences_(std::move(other.in_flight_fences_))
    , current_frame_(other.current_frame_)
    , cleanup_performed_(other.cleanup_performed_)
{
    other.instance_          = VK_NULL_HANDLE;
    other.debug_messenger_   = VK_NULL_HANDLE;
    other.physical_device_   = VK_NULL_HANDLE;
    other.device_            = VK_NULL_HANDLE;
    other.surface_           = VK_NULL_HANDLE;
    other.swapchain_         = VK_NULL_HANDLE;
    other.depth_image_       = VK_NULL_HANDLE;
    other.depth_image_memory_= VK_NULL_HANDLE;
    other.depth_image_view_  = VK_NULL_HANDLE;
    other.render_pass_       = VK_NULL_HANDLE;
    other.command_pool_      = VK_NULL_HANDLE;
    other.current_frame_     = 0;
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
        swapchain_         = other.swapchain_;
        swapchain_images_  = std::move(other.swapchain_images_);
        swapchain_image_views_ = std::move(other.swapchain_image_views_);
        swapchain_format_  = other.swapchain_format_;
        extent_            = other.extent_;
        depth_image_       = other.depth_image_;
        depth_image_memory_= other.depth_image_memory_;
        depth_image_view_  = other.depth_image_view_;
        depth_format_      = other.depth_format_;
        render_pass_       = other.render_pass_;
        framebuffers_      = std::move(other.framebuffers_);
        command_pool_      = other.command_pool_;
        image_available_   = std::move(other.image_available_);
        render_finished_   = std::move(other.render_finished_);
        in_flight_fences_  = std::move(other.in_flight_fences_);
        current_frame_     = other.current_frame_;
        cleanup_performed_ = other.cleanup_performed_;

        other.instance_          = VK_NULL_HANDLE;
        other.debug_messenger_   = VK_NULL_HANDLE;
        other.physical_device_   = VK_NULL_HANDLE;
        other.device_            = VK_NULL_HANDLE;
        other.surface_           = VK_NULL_HANDLE;
        other.swapchain_         = VK_NULL_HANDLE;
        other.depth_image_       = VK_NULL_HANDLE;
        other.depth_image_memory_= VK_NULL_HANDLE;
        other.depth_image_view_  = VK_NULL_HANDLE;
        other.render_pass_       = VK_NULL_HANDLE;
        other.command_pool_      = VK_NULL_HANDLE;
        other.current_frame_     = 0;
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
    create_swapchain();
    create_image_views();
    create_depth_resources();
    create_render_pass();
    create_framebuffers();
    create_command_pool();
    create_sync_objects();

    std::fprintf(stdout, "[INFO]  VulkanContext initialised.\n");
}

void VulkanContext::cleanup() {
    if (cleanup_performed_) return;
    cleanup_performed_ = true;

    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);

        // Framebuffers
        for (auto fb : framebuffers_) {
            vkDestroyFramebuffer(device_, fb, nullptr);
        }
        framebuffers_.clear();

        // Command pool
        if (command_pool_ != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device_, command_pool_, nullptr);
        }

        // Sync objects
        for (auto& s : image_available_)
            vkDestroySemaphore(device_, s, nullptr);
        for (auto& s : render_finished_)
            vkDestroySemaphore(device_, s, nullptr);
        for (auto& f : in_flight_fences_)
            vkDestroyFence(device_, f, nullptr);
        image_available_.clear();
        render_finished_.clear();
        in_flight_fences_.clear();

        // Depth resources
        if (depth_image_view_  != VK_NULL_HANDLE)
            vkDestroyImageView(device_, depth_image_view_, nullptr);
        if (depth_image_       != VK_NULL_HANDLE)
            vkDestroyImage(device_, depth_image_, nullptr);
        if (depth_image_memory_!= VK_NULL_HANDLE)
            vkFreeMemory(device_, depth_image_memory_, nullptr);

        // Render pass
        if (render_pass_ != VK_NULL_HANDLE)
            vkDestroyRenderPass(device_, render_pass_, nullptr);

        // Swapchain image views
        for (auto iv : swapchain_image_views_) {
            vkDestroyImageView(device_, iv, nullptr);
        }
        swapchain_image_views_.clear();

        // Swapchain
        if (swapchain_ != VK_NULL_HANDLE)
            vkDestroySwapchainKHR(device_, swapchain_, nullptr);

        // Device
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    // Debug messenger
    if (debug_messenger_ != VK_NULL_HANDLE) {
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
        if (func) func(instance_, debug_messenger_, nullptr);
        debug_messenger_ = VK_NULL_HANDLE;
    }

    // Instance
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }

    std::fprintf(stdout, "[INFO]  VulkanContext cleaned up.\n");
}

// -----------------------------------------------------------------------
// Instance
// -----------------------------------------------------------------------

void VulkanContext::create_instance() {
    enable_validation_layers_ = check_validation_layer_support();
    if (!enable_validation_layers_) {
        std::fprintf(stderr, "[WARN]  Validation layers not available.\n");
    }

    VkApplicationInfo app{};
    app.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName   = "Map Renderer";
    app.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    app.pEngineName        = "Map Renderer";
    app.engineVersion      = VK_MAKE_API_VERSION(0, 1, 0, 0);
    app.apiVersion         = VK_API_VERSION_1_2;

    auto exts = get_required_extensions();

    VkInstanceCreateInfo info{};
    info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo        = &app;
    info.enabledExtensionCount   = static_cast<uint32_t>(exts.size());
    info.ppEnabledExtensionNames = exts.data();

    if (enable_validation_layers_) {
        info.enabledLayerCount   = static_cast<uint32_t>(validation_layers_.size());
        info.ppEnabledLayerNames = validation_layers_.data();
    }

    if (vkCreateInstance(&info, nullptr, &instance_) != VK_SUCCESS) {
        std::fprintf(stderr, "[ERROR] vkCreateInstance failed.\n");
        std::abort();
    }
}

void VulkanContext::setup_debug_messenger() {
    if (!enable_validation_layers_) return;

    VkDebugUtilsMessengerCreateInfoEXT dbg{};
    dbg.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    dbg.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    dbg.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT    |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT  |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    dbg.pfnUserCallback = debug_callback;

    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
    if (!func) {
        std::fprintf(stderr, "[WARN]  vkCreateDebugUtilsMessengerEXT not found.\n");
        return;
    }
    func(instance_, &dbg, nullptr, &debug_messenger_);
}

// -----------------------------------------------------------------------
// Physical Device
// -----------------------------------------------------------------------

void VulkanContext::pick_physical_device() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (count == 0) {
        std::fprintf(stderr, "[ERROR] No Vulkan-capable devices.\n");
        std::abort();
    }

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());

    for (const auto& dev : devices) {
        if (is_device_suitable(dev)) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(dev, &props);
            std::fprintf(stdout, "[INFO]  GPU: %s (type=%d)\n",
                         props.deviceName, props.deviceType);

            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                physical_device_ = dev;
                break;
            }
            if (physical_device_ == VK_NULL_HANDLE)
                physical_device_ = dev;
        }
    }

    if (physical_device_ == VK_NULL_HANDLE) {
        std::fprintf(stderr, "[ERROR] No suitable physical device.\n");
        std::abort();
    }
}

bool VulkanContext::is_device_suitable(VkPhysicalDevice dev) const {
    auto idx = find_queue_families(dev, surface_);
    if (!idx.is_complete()) return false;

    uint32_t ext_count = 0;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &ext_count, nullptr);
    std::vector<VkExtensionProperties> available(ext_count);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &ext_count,
                                         available.data());

    bool has_swapchain = false;
    for (const auto& ext : available) {
        if (std::strcmp(ext.extensionName,
                        VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            has_swapchain = true;
            break;
        }
    }
    return has_swapchain;
}

VulkanContext::QueueFamilyIndices
VulkanContext::find_queue_families(VkPhysicalDevice dev,
                                   VkSurfaceKHR surface) const
{
    QueueFamilyIndices idx;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, families.data());

    for (uint32_t i = 0; i < count; ++i) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            idx.graphics = i;
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &present);
        if (present) idx.present = i;
        if (idx.is_complete()) break;
    }
    return idx;
}

// -----------------------------------------------------------------------
// Logical Device
// -----------------------------------------------------------------------

void VulkanContext::create_logical_device() {
    auto idx = find_queue_families(physical_device_, surface_);
    graphics_family_ = idx.graphics.value();
    present_family_  = idx.present.value();

    std::set<uint32_t> unique = {graphics_family_, present_family_};
    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> qci;
    for (auto f : unique) {
        VkDeviceQueueCreateInfo q{};
        q.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        q.queueFamilyIndex = f;
        q.queueCount       = 1;
        q.pQueuePriorities = &priority;
        qci.push_back(q);
    }

    const char* exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo info{};
    info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount    = static_cast<uint32_t>(qci.size());
    info.pQueueCreateInfos       = qci.data();
    info.enabledExtensionCount   = 1;
    info.ppEnabledExtensionNames = exts;

    if (enable_validation_layers_) {
        info.enabledLayerCount   = static_cast<uint32_t>(validation_layers_.size());
        info.ppEnabledLayerNames = validation_layers_.data();
    }

    if (vkCreateDevice(physical_device_, &info, nullptr, &device_) != VK_SUCCESS) {
        std::fprintf(stderr, "[ERROR] vkCreateDevice failed.\n");
        std::abort();
    }

    vkGetDeviceQueue(device_, graphics_family_, 0, &graphics_queue_);
    vkGetDeviceQueue(device_, present_family_,  0, &present_queue_);
}

// -----------------------------------------------------------------------
// Swapchain
// -----------------------------------------------------------------------

VkSurfaceFormatKHR VulkanContext::choose_swapchain_format(
    const std::vector<VkSurfaceFormatKHR>& formats) const
{
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return f;
        }
    }
    // Fallback — return the first available
    return formats[0];
}

VkPresentModeKHR VulkanContext::choose_present_mode(
    const std::vector<VkPresentModeKHR>& modes) const
{
    for (auto m : modes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    }
    return VK_PRESENT_MODE_FIFO_KHR;  // guaranteed to exist
}

VkExtent2D VulkanContext::choose_extent(
    const VkSurfaceCapabilitiesKHR& caps) const
{
    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return caps.currentExtent;
    }
    VkExtent2D actual = extent_;
    actual.width  = std::clamp(actual.width,
                               caps.minImageExtent.width,
                               caps.maxImageExtent.width);
    actual.height = std::clamp(actual.height,
                               caps.minImageExtent.height,
                               caps.maxImageExtent.height);
    return actual;
}

void VulkanContext::create_swapchain() {
    auto support = query_swapchain_support(physical_device_, surface_);
    auto fmt     = choose_swapchain_format(support.formats);
    auto mode    = choose_present_mode(support.present_modes);
    auto ex      = choose_extent(support.capabilities);

    swapchain_format_ = fmt.format;
    extent_           = ex;

    uint32_t image_count = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 &&
        image_count > support.capabilities.maxImageCount)
    {
        image_count = support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR info{};
    info.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface          = surface_;
    info.minImageCount    = image_count;
    info.imageFormat      = fmt.format;
    info.imageColorSpace  = fmt.colorSpace;
    info.imageExtent      = ex;
    info.imageArrayLayers = 1;
    info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    auto indices = find_queue_families(physical_device_, surface_);
    uint32_t qfi[] = {indices.graphics.value(), indices.present.value()};

    if (indices.graphics != indices.present) {
        info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices   = qfi;
    } else {
        info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    }

    info.preTransform   = support.capabilities.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode    = mode;
    info.clipped        = VK_TRUE;
    info.oldSwapchain   = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device_, &info, nullptr, &swapchain_) != VK_SUCCESS) {
        std::fprintf(stderr, "[ERROR] vkCreateSwapchainKHR failed.\n");
        std::abort();
    }

    // Retrieve images
    vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, nullptr);
    swapchain_images_.resize(image_count);
    vkGetSwapchainImagesKHR(device_, swapchain_, &image_count,
                            swapchain_images_.data());
}

void VulkanContext::create_image_views() {
    swapchain_image_views_.resize(swapchain_images_.size());
    for (size_t i = 0; i < swapchain_images_.size(); ++i) {
        swapchain_image_views_[i] = create_image_view(
            device_, swapchain_images_[i],
            swapchain_format_,
            VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

// -----------------------------------------------------------------------
// Depth Resources
// -----------------------------------------------------------------------

void VulkanContext::create_depth_resources() {
    // Find a supported depth format (prefer D32_SFLOAT)
    VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };

    depth_format_ = VK_FORMAT_UNDEFINED;
    for (auto fmt : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physical_device_, fmt, &props);
        if (props.optimalTilingFeatures &
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            depth_format_ = fmt;
            break;
        }
    }

    if (depth_format_ == VK_FORMAT_UNDEFINED) {
        std::fprintf(stderr, "[ERROR] No supported depth format.\n");
        std::abort();
    }

    create_image(device_, physical_device_,
                 extent_.width, extent_.height,
                 depth_format_,
                 VK_IMAGE_TILING_OPTIMAL,
                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 depth_image_, depth_image_memory_);

    // Transition depth image to depth attachment layout
    // (we use initialLayout = UNDEFINED, so this transition is needed
    //  at the start of the first render pass via the subpass layout.)

    depth_image_view_ = create_image_view(
        device_, depth_image_, depth_format_,
        VK_IMAGE_ASPECT_DEPTH_BIT);
}

// -----------------------------------------------------------------------
// Render Pass
// -----------------------------------------------------------------------

void VulkanContext::create_render_pass() {
    // Color attachment
    VkAttachmentDescription color_att{};
    color_att.format         = swapchain_format_;
    color_att.samples        = VK_SAMPLE_COUNT_1_BIT;
    color_att.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_att.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    color_att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    color_att.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_ref{};
    color_ref.attachment = 0;
    color_ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth attachment
    VkAttachmentDescription depth_att{};
    depth_att.format         = depth_format_;
    depth_att.samples        = VK_SAMPLE_COUNT_1_BIT;
    depth_att.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_att.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_att.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_ref{};
    depth_ref.attachment = 1;
    depth_ref.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Subpass
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &color_ref;
    subpass.pDepthStencilAttachment = &depth_ref;

    // Dependency
    VkSubpassDependency dep{};
    dep.srcSubpass   = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass   = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[] = {color_att, depth_att};

    VkRenderPassCreateInfo rp_info{};
    rp_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.attachmentCount = 2;
    rp_info.pAttachments    = attachments;
    rp_info.subpassCount    = 1;
    rp_info.pSubpasses      = &subpass;
    rp_info.dependencyCount = 1;
    rp_info.pDependencies   = &dep;

    if (vkCreateRenderPass(device_, &rp_info, nullptr, &render_pass_) != VK_SUCCESS) {
        std::fprintf(stderr, "[ERROR] vkCreateRenderPass failed.\n");
        std::abort();
    }
}

// -----------------------------------------------------------------------
// Framebuffers
// -----------------------------------------------------------------------

void VulkanContext::create_framebuffers() {
    framebuffers_.resize(swapchain_image_views_.size());

    for (size_t i = 0; i < swapchain_image_views_.size(); ++i) {
        VkImageView attachments[] = {
            swapchain_image_views_[i],
            depth_image_view_
        };

        VkFramebufferCreateInfo info{};
        info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass      = render_pass_;
        info.attachmentCount = 2;
        info.pAttachments    = attachments;
        info.width           = extent_.width;
        info.height          = extent_.height;
        info.layers          = 1;

        if (vkCreateFramebuffer(device_, &info, nullptr,
                                &framebuffers_[i]) != VK_SUCCESS)
        {
            std::fprintf(stderr, "[ERROR] vkCreateFramebuffer [%zu] failed.\n", i);
            std::abort();
        }
    }
}

// -----------------------------------------------------------------------
// Command Pool
// -----------------------------------------------------------------------

void VulkanContext::create_command_pool() {
    VkCommandPoolCreateInfo info{};
    info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    info.queueFamilyIndex = graphics_family_;

    if (vkCreateCommandPool(device_, &info, nullptr, &command_pool_) != VK_SUCCESS) {
        std::fprintf(stderr, "[ERROR] vkCreateCommandPool failed.\n");
        std::abort();
    }
}

// -----------------------------------------------------------------------
// Validation / Extension Helpers
// -----------------------------------------------------------------------

bool VulkanContext::check_validation_layer_support() const {
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> available(count);
    vkEnumerateInstanceLayerProperties(&count, available.data());

    for (auto* layer : validation_layers_) {
        bool found = false;
        for (const auto& p : available) {
            if (std::strcmp(layer, p.layerName) == 0) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

std::vector<const char*> VulkanContext::get_required_extensions() const {
    uint32_t count = 0;
    SDL_Vulkan_GetInstanceExtensions(nullptr, &count, nullptr);
    std::vector<const char*> exts(count);
    SDL_Vulkan_GetInstanceExtensions(nullptr, &count, exts.data());

    if (enable_validation_layers_)
        exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    return exts;
}

VulkanContext::SwapchainSupportDetails
VulkanContext::query_swapchain_support(VkPhysicalDevice dev,
                                       VkSurfaceKHR surface)
{
    SwapchainSupportDetails d;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &d.capabilities);

    uint32_t n;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &n, nullptr);
    d.formats.resize(n);
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &n, d.formats.data());

    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &n, nullptr);
    d.present_modes.resize(n);
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &n,
                                              d.present_modes.data());
    return d;
}

// -----------------------------------------------------------------------
// Sync Objects
// -----------------------------------------------------------------------

void VulkanContext::create_sync_objects() {
    image_available_.resize(kMaxFramesInFlight);
    render_finished_.resize(kMaxFramesInFlight);
    in_flight_fences_.resize(kMaxFramesInFlight);

    VkSemaphoreCreateInfo sem{};
    sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence{};
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        if (vkCreateSemaphore(device_, &sem, nullptr, &image_available_[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device_, &sem, nullptr, &render_finished_[i]) != VK_SUCCESS ||
            vkCreateFence(device_, &fence, nullptr, &in_flight_fences_[i]) != VK_SUCCESS)
        {
            std::fprintf(stderr, "[ERROR] Failed to create sync objects.\n");
            std::abort();
        }
    }
}

// -----------------------------------------------------------------------
// Command Buffer Helpers
// -----------------------------------------------------------------------

VkCommandBuffer VulkanContext::allocate_command_buffer() const {
    VkCommandBufferAllocateInfo info{};
    info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool        = command_pool_;
    info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;

    VkCommandBuffer buf;
    vkAllocateCommandBuffers(device_, &info, &buf);
    return buf;
}

VkCommandBuffer VulkanContext::begin_one_time_commands() const {
    auto buf = allocate_command_buffer();

    VkCommandBufferBeginInfo begin{};
    begin.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(buf, &begin);
    return buf;
}

void VulkanContext::end_one_time_commands(VkCommandBuffer cmd) const {
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &cmd;

    vkQueueSubmit(graphics_queue_, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue_);

    vkFreeCommandBuffers(device_, command_pool_, 1, &cmd);
}

// -----------------------------------------------------------------------
// Frame Acquisition + Submission
// -----------------------------------------------------------------------

uint32_t VulkanContext::acquire_next_image() {
    vkWaitForFences(device_, 1, &in_flight_fences_[current_frame_],
                    VK_TRUE, UINT64_MAX);

    uint32_t image_index;
    VkResult result = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                                            image_available_[current_frame_],
                                            VK_NULL_HANDLE, &image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return ~0u;  // signal swapchain re-creation needed
    }

    vkResetFences(device_, 1, &in_flight_fences_[current_frame_]);
    return image_index;
}

void VulkanContext::submit_frame(uint32_t image_index,
                                   void (*record_fn)(VkCommandBuffer, void*),
                                   void* user_data) {
    VkCommandBuffer cmd = allocate_command_buffer();

    // Begin recording
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);

    // Begin render pass with clear values
    VkClearValue clear_values[2];
    clear_values[0].color        = {{0.06f, 0.06f, 0.07f, 1.0f}};  // dark grey
    clear_values[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rp{};
    rp.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass      = render_pass_;
    rp.framebuffer     = framebuffers_[image_index];
    rp.renderArea.offset = {0, 0};
    rp.renderArea.extent = extent_;
    rp.clearValueCount   = 2;
    rp.pClearValues      = clear_values;

    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    if (record_fn) {
        record_fn(cmd, user_data);
    }
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    // Submit
    VkPipelineStageFlags wait_stage =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit{};
    submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount   = 1;
    submit.pWaitSemaphores      = &image_available_[current_frame_];
    submit.pWaitDstStageMask    = &wait_stage;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores    = &render_finished_[current_frame_];
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &cmd;

    vkQueueSubmit(graphics_queue_, 1, &submit,
                  in_flight_fences_[current_frame_]);

    // Present
    VkPresentInfoKHR present{};
    present.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores    = &render_finished_[current_frame_];
    present.swapchainCount     = 1;
    present.pSwapchains        = &swapchain_;
    present.pImageIndices      = &image_index;

    vkQueuePresentKHR(present_queue_, &present);

    // Free command buffer (we allocate a new one each frame for simplicity)
    vkFreeCommandBuffers(device_, command_pool_, 1, &cmd);

    current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;
}

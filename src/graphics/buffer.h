#pragma once

/// @file buffer.h
/// @brief RAII wrapper for a Vulkan buffer with staging upload support.

#include <vulkan/vulkan.h>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>

/// Vulkan buffer with automatic memory allocation.
///
/// Stores the device and physical device handles so all operations
/// (creation, mapping, upload, destruction) are self-contained.
class Buffer {
public:
    Buffer() = default;

    /// Allocate and bind memory.  @p device and @p phys_dev are stored
    /// internally for later use.
    void create(VkDevice device, VkPhysicalDevice phys_dev,
                VkDeviceSize size, VkBufferUsageFlags usage,
                VkMemoryPropertyFlags props);

    ~Buffer() { destroy(); }

    // Move support
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    // Non-copyable
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    /// Free Vulkan resources.
    void destroy();

    /// Copy @p data (size bytes) into the buffer.
    /// If the buffer is HOST_VISIBLE the copy is a direct memcpy.
    /// Otherwise a staging buffer is created and submitted via @p queue.
    /// @pre  Buffer was created with TRANSFER_DST_BIT (device-local) or
    ///       is host-visible.
    void upload(VkQueue queue, VkCommandPool cmd_pool,
                const void* data, VkDeviceSize size);

    // Accessors
    VkBuffer       handle() const { return buffer_; }
    VkDeviceMemory memory() const { return memory_; }
    VkDeviceSize   size()   const { return size_; }

private:
    static uint32_t find_memory_type(VkPhysicalDevice phys_dev,
                                      uint32_t type_filter,
                                      VkMemoryPropertyFlags props);

    VkDevice         device_   = VK_NULL_HANDLE;
    VkPhysicalDevice phys_dev_ = VK_NULL_HANDLE;
    VkBuffer         buffer_   = VK_NULL_HANDLE;
    VkDeviceMemory   memory_   = VK_NULL_HANDLE;
    VkDeviceSize     size_     = 0;
};

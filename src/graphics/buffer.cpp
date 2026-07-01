/// @file buffer.cpp
/// @brief Vulkan buffer implementation.

#include "graphics/buffer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

uint32_t Buffer::find_memory_type(VkPhysicalDevice phys_dev,
                                   uint32_t type_filter,
                                   VkMemoryPropertyFlags props)
{
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(phys_dev, &mp);

    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
        if ((type_filter & (1u << i)) &&
            (mp.memoryTypes[i].propertyFlags & props) == props)
        {
            return i;
        }
    }
    std::fprintf(stderr, "[ERROR] Buffer: no suitable memory type.\n");
    std::abort();
}

// -----------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------

void Buffer::create(VkDevice device, VkPhysicalDevice phys_dev,
                    VkDeviceSize size, VkBufferUsageFlags usage,
                    VkMemoryPropertyFlags props)
{
    device_   = device;
    phys_dev_ = phys_dev;
    size_     = size;

    VkBufferCreateInfo info{};
    info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size        = size;
    info.usage       = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device_, &info, nullptr, &buffer_) != VK_SUCCESS) {
        std::fprintf(stderr, "[ERROR] Buffer::create failed.\n");
        std::abort();
    }

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device_, buffer_, &req);

    VkMemoryAllocateInfo alloc{};
    alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize  = req.size;
    alloc.memoryTypeIndex = find_memory_type(phys_dev_, req.memoryTypeBits, props);

    if (vkAllocateMemory(device_, &alloc, nullptr, &memory_) != VK_SUCCESS) {
        std::fprintf(stderr, "[ERROR] Buffer::create alloc failed.\n");
        std::abort();
    }

    vkBindBufferMemory(device_, buffer_, memory_, 0);
}

Buffer::Buffer(Buffer&& other) noexcept
    : device_(other.device_)
    , phys_dev_(other.phys_dev_)
    , buffer_(other.buffer_)
    , memory_(other.memory_)
    , size_(other.size_)
{
    other.device_   = VK_NULL_HANDLE;
    other.phys_dev_ = VK_NULL_HANDLE;
    other.buffer_   = VK_NULL_HANDLE;
    other.memory_   = VK_NULL_HANDLE;
    other.size_     = 0;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        destroy();
        device_   = other.device_;
        phys_dev_ = other.phys_dev_;
        buffer_   = other.buffer_;
        memory_   = other.memory_;
        size_     = other.size_;
        other.device_   = VK_NULL_HANDLE;
        other.phys_dev_ = VK_NULL_HANDLE;
        other.buffer_   = VK_NULL_HANDLE;
        other.memory_   = VK_NULL_HANDLE;
        other.size_     = 0;
    }
    return *this;
}

void Buffer::destroy() {
    if (device_ != VK_NULL_HANDLE) {
        if (buffer_ != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, buffer_, nullptr);
            buffer_ = VK_NULL_HANDLE;
        }
        if (memory_ != VK_NULL_HANDLE) {
            vkFreeMemory(device_, memory_, nullptr);
            memory_ = VK_NULL_HANDLE;
        }
    }
    size_ = 0;
}

// -----------------------------------------------------------------------
// Upload
// -----------------------------------------------------------------------

void Buffer::upload(VkQueue queue, VkCommandPool cmd_pool,
                    const void* data, VkDeviceSize size)
{
    if (size > size_) size = size_;

    // For host-visible buffers, use a direct map + memcpy
    void* mapped;
    if (vkMapMemory(device_, memory_, 0, size, 0, &mapped) == VK_SUCCESS) {
        std::memcpy(mapped, data, static_cast<size_t>(size));
        VkMappedMemoryRange range{};
        range.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range.memory = memory_;
        range.size   = size;
        vkFlushMappedMemoryRanges(device_, 1, &range);
        vkUnmapMemory(device_, memory_);
        return;
    }

    // Staging buffer for device-local memory
    VkBuffer staging;
    VkDeviceMemory staging_mem;

    VkBufferCreateInfo info{};
    info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size        = size;
    info.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device_, &info, nullptr, &staging) != VK_SUCCESS) {
        std::fprintf(stderr, "[ERROR] Buffer::upload staging create.\n");
        std::abort();
    }

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device_, staging, &req);

    VkMemoryAllocateInfo alloc{};
    alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize  = req.size;
    alloc.memoryTypeIndex = find_memory_type(phys_dev_, req.memoryTypeBits,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device_, &alloc, nullptr, &staging_mem) != VK_SUCCESS) {
        std::fprintf(stderr, "[ERROR] Buffer::upload staging alloc.\n");
        vkDestroyBuffer(device_, staging, nullptr);
        std::abort();
    }
    vkBindBufferMemory(device_, staging, staging_mem, 0);

    // Map staging and copy data
    void* mapped_staging;
    vkMapMemory(device_, staging_mem, 0, size, 0, &mapped_staging);
    std::memcpy(mapped_staging, data, static_cast<size_t>(size));
    vkUnmapMemory(device_, staging_mem);

    // Copy command
    VkCommandBufferAllocateInfo cmd_alloc{};
    cmd_alloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_alloc.commandPool        = cmd_pool;
    cmd_alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_alloc.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device_, &cmd_alloc, &cmd);

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);

    VkBufferCopy region{};
    region.size = size;
    vkCmdCopyBuffer(cmd, staging, buffer_, 1, &region);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &cmd;
    vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device_, cmd_pool, 1, &cmd);
    vkDestroyBuffer(device_, staging, nullptr);
    vkFreeMemory(device_, staging_mem, nullptr);
}

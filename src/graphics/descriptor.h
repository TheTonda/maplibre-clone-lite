#pragma once

/// @file descriptor.h
/// @brief RAII wrapper for a Vulkan descriptor set layout + pool + set.

#include <vulkan/vulkan.h>
#include <vector>

/// Descriptor set + layout + pool in one RAII package.
///
/// Usage:
///   Descriptor desc;
///   desc.create(device, {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
///                         VK_SHADER_STAGE_VERTEX_BIT}});
///   desc.update_buffer(device, binding, buffer_info);
///   // use desc.get_layout() when creating a pipeline
///   // use desc.get_set()  when binding
class Descriptor {
public:
    Descriptor() = default;
    ~Descriptor() { destroy(); }

    Descriptor(Descriptor&& other) noexcept;
    Descriptor& operator=(Descriptor&& other) noexcept;
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;

    /// Create a descriptor set with a single layout + pool + set.
    /// @param bindings  vector of VkDescriptorSetLayoutBinding.
    void create(VkDevice device,
                const std::vector<VkDescriptorSetLayoutBinding>& bindings);

    /// Free resources.
    void destroy();

    /// Update a uniform or storage buffer descriptor at @p binding.
    void update_buffer(VkDevice device, uint32_t binding,
                       const VkDescriptorBufferInfo& buffer_info);

    // Accessors
    VkDescriptorSetLayout layout() const { return layout_; }
    VkDescriptorSet       set()    const { return set_; }
    VkDescriptorPool      pool()   const { return pool_; }

private:
    VkDevice             device_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
    VkDescriptorPool     pool_   = VK_NULL_HANDLE;
    VkDescriptorSet      set_    = VK_NULL_HANDLE;
};

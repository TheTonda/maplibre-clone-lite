/// @file descriptor.cpp
/// @brief Descriptor set wrapper implementation.

#include "graphics/descriptor.h"

#include <cstdio>
#include <cstdlib>

Descriptor::Descriptor(Descriptor&& other) noexcept
    : device_(other.device_)
    , layout_(other.layout_)
    , pool_(other.pool_)
    , set_(other.set_)
{
    other.device_ = VK_NULL_HANDLE;
    other.layout_ = VK_NULL_HANDLE;
    other.pool_   = VK_NULL_HANDLE;
    other.set_    = VK_NULL_HANDLE;
}

Descriptor& Descriptor::operator=(Descriptor&& other) noexcept {
    if (this != &other) {
        destroy();
        device_ = other.device_;
        layout_ = other.layout_;
        pool_   = other.pool_;
        set_    = other.set_;
        other.device_ = VK_NULL_HANDLE;
        other.layout_ = VK_NULL_HANDLE;
        other.pool_   = VK_NULL_HANDLE;
        other.set_    = VK_NULL_HANDLE;
    }
    return *this;
}

void Descriptor::create(VkDevice device,
                        const std::vector<VkDescriptorSetLayoutBinding>& bindings)
{
    device_ = device;

    // --- Layout ---
    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_info.pBindings    = bindings.data();

    if (vkCreateDescriptorSetLayout(device_, &layout_info, nullptr,
                                    &layout_) != VK_SUCCESS)
    {
        std::fprintf(stderr, "[ERROR] Descriptor: layout creation failed.\n");
        std::abort();
    }

    // --- Pool ---
    std::vector<VkDescriptorPoolSize> pool_sizes;
    for (const auto& b : bindings) {
        VkDescriptorPoolSize ps{};
        ps.type            = b.descriptorType;
        ps.descriptorCount = b.descriptorCount;
        pool_sizes.push_back(ps);
    }

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes    = pool_sizes.data();
    pool_info.maxSets       = 1;
    pool_info.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    if (vkCreateDescriptorPool(device_, &pool_info, nullptr,
                               &pool_) != VK_SUCCESS)
    {
        std::fprintf(stderr, "[ERROR] Descriptor: pool creation failed.\n");
        std::abort();
    }

    // --- Set ---
    VkDescriptorSetAllocateInfo alloc{};
    alloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc.descriptorPool     = pool_;
    alloc.descriptorSetCount = 1;
    alloc.pSetLayouts        = &layout_;

    if (vkAllocateDescriptorSets(device_, &alloc, &set_) != VK_SUCCESS) {
        std::fprintf(stderr, "[ERROR] Descriptor: set allocation failed.\n");
        std::abort();
    }
}

void Descriptor::destroy() {
    if (device_ == VK_NULL_HANDLE) return;

    if (pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, pool_, nullptr);
        pool_ = VK_NULL_HANDLE;
    }
    if (layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, layout_, nullptr);
        layout_ = VK_NULL_HANDLE;
    }
    set_ = VK_NULL_HANDLE;
}

void Descriptor::update_buffer(VkDevice device, uint32_t binding,
                               const VkDescriptorBufferInfo& buffer_info)
{
    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = set_;
    write.dstBinding      = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo     = &buffer_info;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

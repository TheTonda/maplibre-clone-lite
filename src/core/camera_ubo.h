#pragma once

/// @file camera_ubo.h
/// @brief Camera uniform buffer object, std140-aligned for GLSL.

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <array>

/// Per-frame camera data uploaded to the GPU as a uniform buffer.
///
/// Must match the GLSL declaration:
///   layout(set = 0, binding = 0) uniform CameraUBO {
///       mat4 proj;
///       mat4 view;
///   };
struct CameraUBO {
    glm::mat4 proj;  ///< Projection matrix (std140: 64 bytes)
    glm::mat4 view;  ///< View matrix       (std140: 64 bytes)

    /// Create a VkDescriptorSetLayoutBinding for binding 0.
    static VkDescriptorSetLayoutBinding layout_binding() {
        VkDescriptorSetLayoutBinding b{};
        b.binding         = 0;
        b.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        b.descriptorCount = 1;
        b.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;
        return b;
    }

    /// Create a VkDescriptorBufferInfo referencing @p buffer.
    static VkDescriptorBufferInfo buffer_info(VkBuffer buffer) {
        VkDescriptorBufferInfo info{};
        info.buffer = buffer;
        info.offset = 0;
        info.range  = sizeof(CameraUBO);
        return info;
    }
};

/// Ensure CameraUBO size matches GLSL expectations (two mat4 = 128 bytes).
static_assert(sizeof(CameraUBO) == 2 * sizeof(glm::mat4),
              "CameraUBO size must match GLSL std140 layout");

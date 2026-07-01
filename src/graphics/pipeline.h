#pragma once

/// @file pipeline.h
/// @brief Graphics pipeline creation from a config struct.

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <unordered_map>

/// Compact description of a graphics pipeline.
///
/// All fields have sensible defaults (fill, back-cull CCW, no depth,
/// no blending) so only non-default values need to be set.
struct PipelineConfig {
    std::string name;                         ///< Debug label / lookup key.

    // Shaders (VkShaderModule handles, loaded externally)
    VkShaderModule vertex_shader   = VK_NULL_HANDLE;
    VkShaderModule fragment_shader = VK_NULL_HANDLE;

    // Vertex input
    std::vector<VkVertexInputBindingDescription>   bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;

    // Rasterisation
    VkPolygonMode  polygon_mode = VK_POLYGON_MODE_FILL;
    VkCullModeFlags cull_mode   = VK_CULL_MODE_BACK_BIT;
    VkFrontFace     front_face  = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    // Depth / stencil
    VkBool32 depth_test  = VK_FALSE;
    VkBool32 depth_write = VK_FALSE;
    VkCompareOp depth_compare = VK_COMPARE_OP_LESS;

    // Blending (per-attachment)
    VkBool32 blend_enable = VK_FALSE;

    // Push constants
    uint32_t push_constant_size = 0;

    // Descriptor set layouts (raw VkDescriptorSetLayout handles)
    std::vector<VkDescriptorSetLayout> descriptor_layouts;
};

/// Manages creation and lookup of Vulkan graphics pipelines.
///
/// Example:
///   PipelineManager mgr;
///   mgr.initialize(device, render_pass);
///   VkPipeline pipe = mgr.create_pipeline(config);
///   VkPipelineLayout layout = mgr.get_layout("ground_2d");
class PipelineManager {
public:
    PipelineManager() = default;
    ~PipelineManager() { cleanup(); }

    PipelineManager(const PipelineManager&) = delete;
    PipelineManager& operator=(const PipelineManager&) = delete;

    PipelineManager(PipelineManager&& other) noexcept;
    PipelineManager& operator=(PipelineManager&& other) noexcept;

    /// Store device and render pass handles.
    void initialize(VkDevice device, VkRenderPass render_pass);

    /// Create a graphics pipeline from @p config.
    /// Returns VK_NULL_HANDLE on failure.
    VkPipeline create_pipeline(const PipelineConfig& config);

    /// Retrieve a previously created pipeline layout by name.
    /// @return VK_NULL_HANDLE if not found.
    VkPipelineLayout get_layout(const std::string& name) const;

    /// Retrieve a previously created pipeline by name.
    /// @return VK_NULL_HANDLE if not found.
    VkPipeline get_pipeline(const std::string& name) const;

    /// Destroy all pipelines and layouts.
    void cleanup();

private:
    VkDevice     device_     = VK_NULL_HANDLE;
    VkRenderPass render_pass_ = VK_NULL_HANDLE;

    std::unordered_map<std::string, VkPipeline>       pipelines_;
    std::unordered_map<std::string, VkPipelineLayout> layouts_;
};

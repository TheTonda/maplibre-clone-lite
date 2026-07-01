#pragma once

/// @file renderer.h
/// @brief Orchestrates geometry → GPU buffers → draw calls for the map.

#include "core/vulkan_context.h"
#include "core/camera.h"
#include "data/geometry_builder.h"
#include "graphics/buffer.h"
#include "graphics/descriptor.h"
#include "graphics/shader.h"
#include "graphics/pipeline.h"

#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

/// The Renderer owns all frame-time resources: descriptor sets, pipelines,
/// shader modules, and geometry buffers.  It provides a
/// record_draw_commands() method that binds everything and issues the
/// appropriate draw calls inside an active render pass.
class Renderer {
public:
    Renderer() = default;
    ~Renderer() { cleanup(); }

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    /// Initialise all pipelines and descriptor resources.
    /// @param ctx  Valid, initialised VulkanContext reference.
    void initialize(VulkanContext& ctx);

    /// Shut down and free all Vulkan resources.
    void cleanup();

    /// Set the camera uniform buffer contents for this frame.
    /// Uploads the UBO data and updates the descriptor set.
    void update_camera(const Camera& camera, float aspect);

    /// Load (or reload) all geometry into device-local buffers.
    void set_geometry(const GeometryData& data);

    /// Record draw commands for the current geometry.
    /// Must be called between vkCmdBeginRenderPass / vkCmdEndRenderPass.
    /// @param cmd           Active command buffer.
    /// @param image_index   Swapchain image index (for descriptor sets).
    void record_draw_commands(VkCommandBuffer cmd, uint32_t image_index);

private:
    // ---- Internal helpers ----
    void create_camera_descriptor();
    void create_ground_pipeline();
    void create_fill_pipeline();
    void create_road_pipeline();
    void create_building_pipeline();

    /// Upload a typed mesh into a vertex+index Buffer pair.
    template<typename T>
    void upload_mesh(const T* vertices, size_t vert_count,
                     const uint32_t* indices, size_t idx_count,
                     Buffer& out_vb, Buffer& out_ib, uint32_t& out_idx_count);

    // ---- References (non-owning) ----
    VkDevice         device_         = VK_NULL_HANDLE;
    VkPhysicalDevice phys_dev_       = VK_NULL_HANDLE;
    VkQueue          graphics_queue_ = VK_NULL_HANDLE;
    VkCommandPool    cmd_pool_       = VK_NULL_HANDLE;
    VkRenderPass     render_pass_    = VK_NULL_HANDLE;

    // ---- Camera ----
    Descriptor   camera_descriptor_;
    Buffer       camera_buffer_;

    // ---- Pipelines ----
    ShaderManager    shader_mgr_;
    PipelineManager  pipeline_mgr_;

    // ---- Geometry buffers (per-mesh-type) ----
    Buffer     ground_vb_, ground_ib_;
    uint32_t   ground_idx_count_ = 0;

    Buffer     fill_vb_, fill_ib_;
    uint32_t   fill_idx_count_ = 0;

    Buffer     road_vb_, road_ib_;
    uint32_t   road_idx_count_ = 0;

    Buffer     building_vb_, building_ib_;
    uint32_t   building_idx_count_ = 0;

    bool initialized_ = false;
};

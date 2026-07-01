/// @file renderer.cpp
/// @brief Renderer implementation.

#include "render/renderer.h"

#include "graphics/buffer.h"
#include "graphics/pipeline.h"
#include "core/camera_ubo.h"

#include <cstring>
#include <cstdio>
#include <filesystem>
#include <string>

/// Build a shader path that resolves regardless of the working directory.
/// Tries MAP_RENDERER_SHADER_DIR first (absolute, set by CMake), then falls
/// back to common relative locations.
static std::string shader_path(const std::string& rel) {
    namespace fs = std::filesystem;

    // 1. Absolute path from CMake compile definition
    std::string abs_from_def = std::string(MAP_RENDERER_SHADER_DIR) + "/" + rel;
    if (fs::exists(abs_from_def)) return abs_from_def;

    // 2. Running from project root: _build/shaders/...
    std::string build_rel = "_build/shaders/" + rel;
    if (fs::exists(build_rel)) return build_rel;

    // 3. Running from _build/ directory: shaders/...
    std::string direct = "shaders/" + rel;
    if (fs::exists(direct)) return direct;

    // Nothing found — return the absolute one so the error message is clearer
    return abs_from_def;
}

// ======================================================================
// Initialisation & Cleanup
// ======================================================================

template<typename T>
void Renderer::upload_mesh(const T* vertices, size_t vert_count,
                            const uint32_t* indices, size_t idx_count,
                            Buffer& out_vb, Buffer& out_ib,
                            uint32_t& out_idx_count)
{
    VkDeviceSize vb_size = static_cast<VkDeviceSize>(vert_count) * sizeof(T);
    VkDeviceSize ib_size = static_cast<VkDeviceSize>(idx_count) * sizeof(uint32_t);

    // Destroy previous buffers
    out_vb.destroy();
    out_ib.destroy();

    if (vert_count == 0 || idx_count == 0) {
        out_idx_count = 0;
        return;
    }

    out_vb.create(device_, phys_dev_, vb_size,
                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    out_vb.upload(graphics_queue_, cmd_pool_, vertices, vb_size);

    out_ib.create(device_, phys_dev_, ib_size,
                  VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    out_ib.upload(graphics_queue_, cmd_pool_, indices, ib_size);

    out_idx_count = static_cast<uint32_t>(idx_count);
}

// ======================================================================

void Renderer::initialize(VulkanContext& ctx) {
    device_         = ctx.get_device();
    phys_dev_       = ctx.get_physical_device();
    graphics_queue_ = ctx.get_graphics_queue();
    cmd_pool_       = ctx.get_command_pool();
    render_pass_    = ctx.get_render_pass();

    // Shader manager
    shader_mgr_.initialize(device_);

    // Pipeline manager
    pipeline_mgr_.initialize(device_, render_pass_);

    // Camera uniform buffer (one per frame, host-visible)
    VkDeviceSize ubo_size = sizeof(CameraUBO);
    camera_buffer_.create(device_, phys_dev_, ubo_size,
                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // Load shader modules (SPIR-V compiled by tools/compile_shaders.sh)
    shader_mgr_.load_from_file(shader_path("2d/ground.vert.spv"));
    shader_mgr_.load_from_file(shader_path("2d/ground.frag.spv"));
    shader_mgr_.load_from_file(shader_path("2d/fill.vert.spv"));
    shader_mgr_.load_from_file(shader_path("2d/fill.frag.spv"));
    shader_mgr_.load_from_file(shader_path("3d/building.vert.spv"));
    shader_mgr_.load_from_file(shader_path("3d/building.frag.spv"));

    // Create descriptor set for camera UBO
    create_camera_descriptor();

    // Build pipelines
    create_ground_pipeline();
    create_fill_pipeline();
    create_road_pipeline();
    create_building_pipeline();

    // Diagnostic: confirm pipelines were created
    std::fprintf(stdout, "[Renderer] Pipelines ready: ground=%p fill=%p road=%p building=%p\n",
                 (void*)pipeline_mgr_.get_pipeline("ground_2d"),
                 (void*)pipeline_mgr_.get_pipeline("fill_2d"),
                 (void*)pipeline_mgr_.get_pipeline("road_2d"),
                 (void*)pipeline_mgr_.get_pipeline("building_3d"));

    initialized_ = true;
}

void Renderer::cleanup() {
    if (!initialized_) return;

    vkDeviceWaitIdle(device_);

    building_vb_.destroy(); building_ib_.destroy();
    road_vb_.destroy();     road_ib_.destroy();
    fill_vb_.destroy();     fill_ib_.destroy();
    ground_vb_.destroy();   ground_ib_.destroy();

    pipeline_mgr_.cleanup();
    shader_mgr_.cleanup();
    camera_descriptor_.destroy();
    camera_buffer_.destroy();

    initialized_ = false;
}

// ======================================================================
// Camera
// ======================================================================

void Renderer::create_camera_descriptor() {
    auto binding = CameraUBO::layout_binding();
    camera_descriptor_.create(device_, {binding});

    VkDescriptorBufferInfo buf_info = CameraUBO::buffer_info(camera_buffer_.handle());
    camera_descriptor_.update_buffer(device_, 0, buf_info);
}

void Renderer::update_camera(const Camera& camera, float aspect) {
    CameraUBO ubo;
    ubo.proj = camera.get_projection_matrix(aspect);
    ubo.view = camera.get_view_matrix();

    // Upload to host-visible coherent buffer
    void* mapped;
    vkMapMemory(device_, camera_buffer_.memory(), 0, sizeof(CameraUBO), 0, &mapped);
    std::memcpy(mapped, &ubo, sizeof(CameraUBO));
    vkUnmapMemory(device_, camera_buffer_.memory());
}

// ======================================================================
// Pipelines
// ======================================================================

void Renderer::create_ground_pipeline() {
    VkShaderModule vert = shader_mgr_.load_from_file(shader_path("2d/ground.vert.spv"));
    VkShaderModule frag = shader_mgr_.load_from_file(shader_path("2d/ground.frag.spv"));
    if (!vert || !frag) {
        std::fprintf(stderr, "[Renderer] Failed to load ground shaders\n");
        return;
    }

    // FillVertex: vec2 pos
    VkVertexInputBindingDescription binding{};
    binding.binding   = 0;
    binding.stride    = sizeof(FillVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attr{};
    attr.location = 0;
    attr.binding  = 0;
    attr.format   = VK_FORMAT_R32G32_SFLOAT;
    attr.offset   = offsetof(FillVertex, pos);

    PipelineConfig cfg{};
    cfg.name              = "ground_2d";
    cfg.vertex_shader     = vert;
    cfg.fragment_shader   = frag;
    cfg.bindings          = {binding};
    cfg.attributes        = {attr};
    cfg.depth_test        = VK_FALSE;
    cfg.depth_write       = VK_FALSE;
    cfg.descriptor_layouts = {camera_descriptor_.layout()};

    pipeline_mgr_.create_pipeline(cfg);
}

void Renderer::create_fill_pipeline() {
    VkShaderModule vert = shader_mgr_.load_from_file(shader_path("2d/fill.vert.spv"));
    VkShaderModule frag = shader_mgr_.load_from_file(shader_path("2d/fill.frag.spv"));
    if (!vert || !frag) {
        std::fprintf(stderr, "[Renderer] Failed to load fill shaders\n");
        return;
    }

    VkVertexInputBindingDescription binding{};
    binding.binding   = 0;
    binding.stride    = sizeof(FillVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attr{};
    attr.location = 0;
    attr.binding  = 0;
    attr.format   = VK_FORMAT_R32G32_SFLOAT;
    attr.offset   = offsetof(FillVertex, pos);

    PipelineConfig cfg{};
    cfg.name               = "fill_2d";
    cfg.vertex_shader      = vert;
    cfg.fragment_shader    = frag;
    cfg.bindings           = {binding};
    cfg.attributes         = {attr};
    cfg.depth_test         = VK_FALSE;
    cfg.depth_write        = VK_FALSE;
    cfg.push_constant_size = 16;   // vec4 color
    cfg.descriptor_layouts = {camera_descriptor_.layout()};

    pipeline_mgr_.create_pipeline(cfg);
}

void Renderer::create_road_pipeline() {
    // Re-use fill shaders for now (road vertices are 3D but rendered in 2D pass).
    // In later tasks we might add a dedicated road.vert with colour/UV support.
    VkShaderModule vert = shader_mgr_.load_from_file(shader_path("2d/fill.vert.spv"));
    VkShaderModule frag = shader_mgr_.load_from_file(shader_path("2d/fill.frag.spv"));
    if (!vert || !frag) return;

    // RoadVertex: vec3 pos (used as (x, y=0, z) for 2D ortho)
    VkVertexInputBindingDescription binding{};
    binding.binding   = 0;
    binding.stride    = sizeof(RoadVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attr{};
    attr.location = 0;
    attr.binding  = 0;
    attr.format   = VK_FORMAT_R32G32B32_SFLOAT;
    attr.offset   = offsetof(RoadVertex, pos);

    PipelineConfig cfg{};
    cfg.name               = "road_2d";
    cfg.vertex_shader      = vert;
    cfg.fragment_shader    = frag;
    cfg.bindings           = {binding};
    cfg.attributes         = {attr};
    cfg.depth_test         = VK_FALSE;
    cfg.depth_write        = VK_FALSE;
    cfg.push_constant_size = 16;
    cfg.descriptor_layouts = {camera_descriptor_.layout()};

    pipeline_mgr_.create_pipeline(cfg);
}

void Renderer::create_building_pipeline() {
    VkShaderModule vert = shader_mgr_.load_from_file(shader_path("3d/building.vert.spv"));
    VkShaderModule frag = shader_mgr_.load_from_file(shader_path("3d/building.frag.spv"));
    if (!vert || !frag) {
        std::fprintf(stderr, "[Renderer] Failed to load building shaders\n");
        return;
    }

    // BuildingVertex: vec3 pos + vec3 normal
    VkVertexInputBindingDescription binding{};
    binding.binding   = 0;
    binding.stride    = sizeof(BuildingVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> attrs(2);
    attrs[0].location = 0;
    attrs[0].binding  = 0;
    attrs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset   = offsetof(BuildingVertex, pos);
    attrs[1].location = 1;
    attrs[1].binding  = 0;
    attrs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset   = offsetof(BuildingVertex, normal);

    PipelineConfig cfg{};
    cfg.name               = "building_3d";
    cfg.vertex_shader      = vert;
    cfg.fragment_shader    = frag;
    cfg.bindings           = {binding};
    cfg.attributes         = {attrs};
    cfg.depth_test         = VK_TRUE;
    cfg.depth_write        = VK_TRUE;
    cfg.push_constant_size = 16;
    cfg.descriptor_layouts = {camera_descriptor_.layout()};

    pipeline_mgr_.create_pipeline(cfg);
}

// ======================================================================
// Geometry
// ======================================================================

void Renderer::set_geometry(const GeometryData& data) {
    // Ground
    upload_mesh(data.ground.vertices.data(), data.ground.vertices.size(),
                data.ground.indices.data(), data.ground.indices.size(),
                ground_vb_, ground_ib_, ground_idx_count_);

    // Fill polygons (parks + water + landuse — draw as one batch)
    {
        std::vector<FillVertex>  all_fill_v;
        std::vector<uint32_t>    all_fill_i;
        uint32_t base = 0;
        auto append = [&](const std::vector<FillVertex>& verts,
                          const std::vector<uint32_t>& idxs) {
            for (auto& v : verts) all_fill_v.push_back(v);
            for (auto idx : idxs) all_fill_i.push_back(idx + base);
            base += static_cast<uint32_t>(verts.size());
        };
        append(data.parks.vertices, data.parks.indices);
        append(data.water.vertices, data.water.indices);
        append(data.landuse.vertices, data.landuse.indices);

        upload_mesh(all_fill_v.data(), all_fill_v.size(),
                    all_fill_i.data(), all_fill_i.size(),
                    fill_vb_, fill_ib_, fill_idx_count_);
    }

    // Roads (all types — one batch)
    {
        std::vector<RoadVertex>  all_road_v;
        std::vector<uint32_t>    all_road_i;
        uint32_t base = 0;
        auto append = [&](const std::vector<RoadVertex>& verts,
                          const std::vector<uint32_t>& idxs) {
            for (auto& v : verts) all_road_v.push_back(v);
            for (auto idx : idxs) all_road_i.push_back(idx + base);
            base += static_cast<uint32_t>(verts.size());
        };
        append(data.roads_primary.vertices,   data.roads_primary.indices);
        append(data.roads_secondary.vertices, data.roads_secondary.indices);
        append(data.roads_residential.vertices, data.roads_residential.indices);
        append(data.roads_service.vertices,   data.roads_service.indices);

        upload_mesh(all_road_v.data(), all_road_v.size(),
                    all_road_i.data(), all_road_i.size(),
                    road_vb_, road_ib_, road_idx_count_);
    }

    // Buildings
    upload_mesh(data.buildings.vertices.data(), data.buildings.vertices.size(),
                data.buildings.indices.data(), data.buildings.indices.size(),
                building_vb_, building_ib_, building_idx_count_);

    std::fprintf(stdout, "[Renderer] Geometry set: ground=%u tris fill=%u road=%u building=%u\n",
                 ground_idx_count_ / 3, fill_idx_count_ / 3,
                 road_idx_count_ / 3, building_idx_count_ / 3);
}

// ======================================================================
// Draw Commands
// ======================================================================

void Renderer::record_draw_commands(VkCommandBuffer cmd, uint32_t /*image_index*/) {
    VkDescriptorSet cam_set = camera_descriptor_.set();

    // ---- Bind camera descriptor (set 0 for all pipelines) ----
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_mgr_.get_layout("ground_2d"),
                            0, 1, &cam_set,
                            0, nullptr);

    // ================================================================
    // 1. Ground plane (no depth, no push constants)
    // ================================================================
    if (ground_idx_count_ > 0) {
        VkPipeline ground_pipe = pipeline_mgr_.get_pipeline("ground_2d");
        VkPipelineLayout ground_layout = pipeline_mgr_.get_layout("ground_2d");
        if (ground_pipe && ground_layout) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ground_pipe);

            VkBuffer vbs[] = {ground_vb_.handle()};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
            vkCmdBindIndexBuffer(cmd, ground_ib_.handle(), 0, VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(cmd, ground_idx_count_, 1, 0, 0, 0);
        }
    }

    // ================================================================
    // 2. Fill polygons (parks, water, landuse — 2D, push constant color)
    // ================================================================
    if (fill_idx_count_ > 0) {
        VkPipeline fill_pipe = pipeline_mgr_.get_pipeline("fill_2d");
        VkPipelineLayout fill_layout = pipeline_mgr_.get_layout("fill_2d");
        if (fill_pipe && fill_layout) {
            // Need to re-bind descriptor set because fill pipeline uses a
            // different layout (it has push constants the ground doesn't).
            // Actually ground and fill use the same layout: set=0 + push=16.
            // But to be safe:
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    fill_layout,
                                    0, 1, &cam_set,
                                    0, nullptr);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fill_pipe);

            VkBuffer vbs[] = {fill_vb_.handle()};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
            vkCmdBindIndexBuffer(cmd, fill_ib_.handle(), 0, VK_INDEX_TYPE_UINT32);

            // Push constant: white colour
            struct { float r, g, b, a; } white = {0.9f, 0.9f, 0.9f, 1.0f};
            vkCmdPushConstants(cmd, fill_layout,
                               VK_SHADER_STAGE_VERTEX_BIT,
                               0, sizeof(white), &white);

            vkCmdDrawIndexed(cmd, fill_idx_count_, 1, 0, 0, 0);
        }
    }

    // ================================================================
    // 3. Roads (2D, push constant color — grey)
    // ================================================================
    if (road_idx_count_ > 0) {
        VkPipeline road_pipe = pipeline_mgr_.get_pipeline("road_2d");
        VkPipelineLayout road_layout = pipeline_mgr_.get_layout("road_2d");
        if (road_pipe && road_layout) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    road_layout,
                                    0, 1, &cam_set,
                                    0, nullptr);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, road_pipe);

            VkBuffer vbs[] = {road_vb_.handle()};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
            vkCmdBindIndexBuffer(cmd, road_ib_.handle(), 0, VK_INDEX_TYPE_UINT32);

            struct { float r, g, b, a; } road_color = {0.4f, 0.4f, 0.4f, 1.0f};
            vkCmdPushConstants(cmd, road_layout,
                               VK_SHADER_STAGE_VERTEX_BIT,
                               0, sizeof(road_color), &road_color);

            vkCmdDrawIndexed(cmd, road_idx_count_, 1, 0, 0, 0);
        }
    }

    // ================================================================
    // 4. Buildings (with depth testing — flat colour)
    // ================================================================
    if (building_idx_count_ > 0) {
        VkPipeline bldg_pipe = pipeline_mgr_.get_pipeline("building_3d");
        VkPipelineLayout bldg_layout = pipeline_mgr_.get_layout("building_3d");
        if (bldg_pipe && bldg_layout) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    bldg_layout,
                                    0, 1, &cam_set,
                                    0, nullptr);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bldg_pipe);

            VkBuffer vbs[] = {building_vb_.handle()};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
            vkCmdBindIndexBuffer(cmd, building_ib_.handle(), 0, VK_INDEX_TYPE_UINT32);

            struct { float r, g, b, a; } bldg_color = {0.85f, 0.76f, 0.65f, 1.0f};
            vkCmdPushConstants(cmd, bldg_layout,
                               VK_SHADER_STAGE_VERTEX_BIT,
                               0, sizeof(bldg_color), &bldg_color);

            vkCmdDrawIndexed(cmd, building_idx_count_, 1, 0, 0, 0);
        }
    }
}

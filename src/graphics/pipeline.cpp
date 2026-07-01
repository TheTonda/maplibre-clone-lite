/// @file pipeline.cpp
/// @brief Graphics pipeline creation.

#include "graphics/pipeline.h"

#include <cstdio>
#include <cstdlib>

PipelineManager::PipelineManager(PipelineManager&& other) noexcept
    : device_(other.device_)
    , render_pass_(other.render_pass_)
    , pipelines_(std::move(other.pipelines_))
    , layouts_(std::move(other.layouts_))
{
    other.device_     = VK_NULL_HANDLE;
    other.render_pass_ = VK_NULL_HANDLE;
}

PipelineManager& PipelineManager::operator=(PipelineManager&& other) noexcept {
    if (this != &other) {
        cleanup();
        device_     = other.device_;
        render_pass_ = other.render_pass_;
        pipelines_   = std::move(other.pipelines_);
        layouts_     = std::move(other.layouts_);
        other.device_     = VK_NULL_HANDLE;
        other.render_pass_ = VK_NULL_HANDLE;
    }
    return *this;
}

void PipelineManager::initialize(VkDevice device, VkRenderPass render_pass) {
    device_     = device;
    render_pass_ = render_pass;
}

VkPipeline PipelineManager::create_pipeline(const PipelineConfig& config) {
    // --- Shader stages ---
    VkPipelineShaderStageCreateInfo stages[2] = {};

    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = config.vertex_shader;
    stages[0].pName  = "main";

    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = config.fragment_shader;
    stages[1].pName  = "main";

    // --- Vertex input ---
    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount   = static_cast<uint32_t>(config.bindings.size());
    vi.pVertexBindingDescriptions      = config.bindings.data();
    vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(config.attributes.size());
    vi.pVertexAttributeDescriptions    = config.attributes.data();

    // --- Input assembly ---
    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // --- Viewport (dynamic) ---
    VkPipelineViewportStateCreateInfo vp{};
    vp.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount  = 1;

    // --- Rasterisation ---
    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = config.polygon_mode;
    rs.cullMode    = config.cull_mode;
    rs.frontFace   = config.front_face;
    rs.lineWidth   = 1.0f;

    // --- Multisampling ---
    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // --- Depth / stencil ---
    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable       = config.depth_test;
    ds.depthWriteEnable      = config.depth_write;
    ds.depthCompareOp        = config.depth_compare;
    ds.depthBoundsTestEnable = VK_FALSE;
    ds.stencilTestEnable     = VK_FALSE;

    // --- Color blending ---
    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend.blendEnable = config.blend_enable;
    if (config.blend_enable) {
        blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend.colorBlendOp        = VK_BLEND_OP_ADD;
        blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blend.alphaBlendOp        = VK_BLEND_OP_ADD;
    }

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments    = &blend;

    // --- Dynamic state ---
    VkDynamicState dyn[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dy{};
    dy.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dy.dynamicStateCount = 2;
    dy.pDynamicStates    = dyn;

    // --- Push constants ---
    VkPushConstantRange push_range{};
    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    if (config.push_constant_size > 0) {
        push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        push_range.offset     = 0;
        push_range.size       = config.push_constant_size;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges    = &push_range;
    }

    layout_info.setLayoutCount = static_cast<uint32_t>(config.descriptor_layouts.size());
    layout_info.pSetLayouts    = config.descriptor_layouts.data();

    VkPipelineLayout layout;
    if (vkCreatePipelineLayout(device_, &layout_info, nullptr, &layout) != VK_SUCCESS) {
        std::fprintf(stderr, "[ERROR] Pipeline: layout '%s' failed.\n", config.name.c_str());
        return VK_NULL_HANDLE;
    }

    // --- Graphics pipeline ---
    VkGraphicsPipelineCreateInfo info{};
    info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount          = 2;
    info.pStages             = stages;
    info.pVertexInputState   = &vi;
    info.pInputAssemblyState = &ia;
    info.pViewportState      = &vp;
    info.pRasterizationState = &rs;
    info.pMultisampleState   = &ms;
    info.pDepthStencilState  = &ds;
    info.pColorBlendState    = &cb;
    info.pDynamicState       = &dy;
    info.layout              = layout;
    info.renderPass          = render_pass_;
    info.subpass             = 0;

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info,
                                  nullptr, &pipeline) != VK_SUCCESS)
    {
        std::fprintf(stderr, "[ERROR] Pipeline: '%s' creation failed.\n", config.name.c_str());
        vkDestroyPipelineLayout(device_, layout, nullptr);
        return VK_NULL_HANDLE;
    }

    pipelines_[config.name] = pipeline;
    layouts_[config.name]   = layout;
    return pipeline;
}

VkPipelineLayout PipelineManager::get_layout(const std::string& name) const {
    auto it = layouts_.find(name);
    return (it != layouts_.end()) ? it->second : VK_NULL_HANDLE;
}

VkPipeline PipelineManager::get_pipeline(const std::string& name) const {
    auto it = pipelines_.find(name);
    return (it != pipelines_.end()) ? it->second : VK_NULL_HANDLE;
}

void PipelineManager::cleanup() {
    if (device_ == VK_NULL_HANDLE) return;

    for (auto& [_, p] : pipelines_)
        vkDestroyPipeline(device_, p, nullptr);
    pipelines_.clear();

    for (auto& [_, l] : layouts_)
        vkDestroyPipelineLayout(device_, l, nullptr);
    layouts_.clear();
}

#pragma once

/// @file shader.h
/// @brief Loads SPIR-V shader modules from files or embedded source.

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

/// Loads and caches SPIR-V shader modules.
///
/// Example:
///   ShaderManager mgr;
///   mgr.initialize(device);
///   VkShaderModule mod = mgr.load_from_file("shaders/2d/fill.vert.spv");
class ShaderManager {
public:
    ShaderManager() = default;
    ~ShaderManager() { cleanup(); }

    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;

    ShaderManager(ShaderManager&& other) noexcept;
    ShaderManager& operator=(ShaderManager&& other) noexcept;

    /// Set the device handle (must be called before loading).
    void initialize(VkDevice device) { device_ = device; }

    /// Load a SPIR-V binary from file and create a VkShaderModule.
    /// Returns VK_NULL_HANDLE on failure.
    VkShaderModule load_from_file(const std::string& path);

    /// Create a shader module from raw SPIR-V code.
    VkShaderModule load_from_source(const uint32_t* code, size_t word_count);

    /// Free all loaded modules and the manager.
    void cleanup();

private:
    VkDevice device_ = VK_NULL_HANDLE;

    /// Kept alive so they can be freed in cleanup().
    std::vector<VkShaderModule> modules_;
};

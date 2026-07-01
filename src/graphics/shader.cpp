/// @file shader.cpp
/// @brief Shader module loading.

#include "graphics/shader.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>

ShaderManager::ShaderManager(ShaderManager&& other) noexcept
    : device_(other.device_)
    , modules_(std::move(other.modules_))
{
    other.device_ = VK_NULL_HANDLE;
}

ShaderManager& ShaderManager::operator=(ShaderManager&& other) noexcept {
    if (this != &other) {
        cleanup();
        device_ = other.device_;
        modules_ = std::move(other.modules_);
        other.device_ = VK_NULL_HANDLE;
    }
    return *this;
}

VkShaderModule ShaderManager::load_from_file(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::fprintf(stderr, "[ERROR] Shader: cannot open '%s'\n", path.c_str());
        return VK_NULL_HANDLE;
    }

    size_t file_size = static_cast<size_t>(file.tellg());
    std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()),
              static_cast<std::streamsize>(file_size));
    file.close();

    return load_from_source(buffer.data(), buffer.size());
}

VkShaderModule ShaderManager::load_from_source(const uint32_t* code,
                                                size_t word_count)
{
    VkShaderModuleCreateInfo info{};
    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = word_count * sizeof(uint32_t);
    info.pCode    = code;

    VkShaderModule mod;
    if (vkCreateShaderModule(device_, &info, nullptr, &mod) != VK_SUCCESS) {
        std::fprintf(stderr, "[ERROR] Shader: module creation failed.\n");
        return VK_NULL_HANDLE;
    }

    modules_.push_back(mod);
    return mod;
}

void ShaderManager::cleanup() {
    if (device_ != VK_NULL_HANDLE) {
        for (auto m : modules_) {
            vkDestroyShaderModule(device_, m, nullptr);
        }
        modules_.clear();
    }
}

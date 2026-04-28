#include "Shader.h"
#include "Utils.h"

#include <iostream>
#include <hilog/log.h>
// 接入要求的日志宏
#undef LOG_TAG
#define LOG_TAG "Shader"

#undef LOG_DOMAIN
#define LOG_DOMAIN 0xFF06

#define LOGI(...) OH_LOG_INFO(LOG_APP, __VA_ARGS__)
#define LOGE(...) OH_LOG_ERROR(LOG_APP, __VA_ARGS__)

Shader::~Shader() {
    if (vkShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(context->device_, vkShaderModule, nullptr);
        vkShaderModule = VK_NULL_HANDLE;
    }
}

void Shader::load() {
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

    std::vector<char> shader_code; // 用于持有读取的文件内容，保证内存生命周期

    if (data == nullptr) {
        auto fn = "shaders/" + filename + ".spv";
        // 注意：Utils::readFile 需要返回 std::vector<char>
        shader_code = VulkanUtils::readFile(context->config_.resourceManager_, fn);
        
        if (shader_code.empty()) {
            LOGE("Failed to load shader file: %{public}s", fn.c_str());
            throw std::runtime_error("Failed to load shader: " + fn);
        }
        
        create_info.codeSize = shader_code.size();
        create_info.pCode = reinterpret_cast<const uint32_t *>(shader_code.data());
        LOGI("Shader loaded from file: %{public}s (Size: %{public}zu)", fn.c_str(), shader_code.size());
    } else {
        create_info.codeSize = size;
        create_info.pCode = reinterpret_cast<const uint32_t *>(data);
        LOGI("Shader loaded from memory (Size: %{public}zu)", size);
    }

    // 使用 C 风格 API 创建 ShaderModule
    VkResult res = vkCreateShaderModule(context->device_, &create_info, nullptr, &vkShaderModule);
    
    if (res != VK_SUCCESS) {
        LOGE("Failed to create shader module, VkResult: %{public}d", res);
        throw std::runtime_error("Failed to create shader module");
    }

    // 如果你有 Debug 扩展，可以在这里设置名称 (可选)
    // 鸿蒙环境下如果没开启 Validation Layer，这一部分可以省略
    /*
    if (vkShaderModule != VK_NULL_HANDLE && !filename.empty()) {
        // C 风格设置 Debug 名称逻辑
    }
    */
}
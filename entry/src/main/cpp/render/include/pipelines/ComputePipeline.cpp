#include "ComputePipeline.h"

ComputePipeline::ComputePipeline(const std::shared_ptr<Context>& context, std::shared_ptr<Shader> shader)
    : Pipeline(context), shader(std::move(shader)) {
    // 假设你的 Shader 类现在也改为了 C 风格，内部通过 vkCreateShaderModule 加载
    this->shader->load(); 
}

void ComputePipeline::build() {
    buildPipelineLayout();

    // 填充着色器阶段信息
    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = shader->vkShaderModule; // 假设 Shader 类提供 VkShaderModule 句柄
    stageInfo.pName = "main";

    // 填充计算管线信息
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.layout = pipelineLayout;

    if (vkCreateComputePipelines(context->device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute pipeline");
    }
}
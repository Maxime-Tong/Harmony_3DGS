#include "Pipeline.h"

#include <utility>

uint32_t Pipeline::DescriptorOption::get(size_t index) const {
    return multiple ? values[index] : value;
}

Pipeline::Pipeline(const std::shared_ptr<Context>& _context) : context(_context) {}

Pipeline::~Pipeline() {
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(context->device_, pipeline, nullptr);
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context->device_, pipelineLayout, nullptr);
    }
}

void Pipeline::addDescriptorSet(uint32_t set, std::shared_ptr<DescriptorSet> descriptorSet) {
    descriptorSets[set] = std::move(descriptorSet);
}

void Pipeline::addPushConstant(VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size) {
    VkPushConstantRange range{};
    range.stageFlags = stageFlags;
    range.offset = offset;
    range.size = size;
    pushConstantRanges.push_back(range);
}

void Pipeline::buildPipelineLayout() {
    std::vector<VkDescriptorSetLayout> layouts;
    
    layouts.reserve(descriptorSets.size());
    for (auto &descriptorSet : descriptorSets) {
        layouts.push_back(descriptorSet.second->descriptorSetLayout);
    }

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
    layoutInfo.pSetLayouts = layouts.data();
    layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
    layoutInfo.pPushConstantRanges = pushConstantRanges.empty() ? nullptr : pushConstantRanges.data();

    if (vkCreatePipelineLayout(context->device_, &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }
}

void Pipeline::bind(VkCommandBuffer commandBuffer, uint8_t currentFrame, DescriptorOption option) {
    // 绑定 Pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

    // 准备描述符集
    std::vector<VkDescriptorSet> descriptorSetsToBind;
    uint32_t i = 0;
    for (auto &ds : descriptorSets) {
        descriptorSetsToBind.push_back(ds.second->getDescriptorSet(currentFrame, option.get(i++)));
    }

    if (!descriptorSetsToBind.empty()) {
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 
                                0, static_cast<uint32_t>(descriptorSetsToBind.size()), 
                                descriptorSetsToBind.data(), 0, nullptr);
    }
}
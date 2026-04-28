//
// Created on 2026/4/8.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "DescriptorSet.h"

#include <hilog/log.h>

#undef LOG_TAG
#define LOG_TAG "DescriptorSet"

#undef LOG_DOMAIN
#define LOG_DOMAIN 0xFF05

#define LOGI(...) OH_LOG_INFO(LOG_APP, __VA_ARGS__)
#define LOGE(...) OH_LOG_ERROR(LOG_APP, __VA_ARGS__)

DescriptorSet::DescriptorSet(const std::shared_ptr<Context>& _context, uint8_t framesInFlight) 
    : context(_context), framesInFlight(framesInFlight) {
}

DescriptorSet::~DescriptorSet() {
    if (descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(context->device_, descriptorSetLayout, nullptr);
    }
    // 注意：DescriptorSet 是从 Pool 分配的，通常随 Pool 销毁，除非设置了 FREE_BIT
}

void DescriptorSet::bindBufferToDescriptorSet(uint32_t binding, VkDescriptorType type, VkShaderStageFlagBits stage,
                                              std::shared_ptr<Buffer> buffer) {
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = binding;
    layoutBinding.descriptorType = type;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = stage;
    layoutBinding.pImmutableSamplers = nullptr;

    const auto& bindingVector = bindings[binding];
    if (!bindingVector.empty() && bindingVector[bindingVector.size()-1].layoutBinding.descriptorType != type) {
        LOGE("Binding already exists with different type");
    }

    DescriptorBinding db{};
    db.type = type;
    db.layoutBinding = layoutBinding;

    db.buffer = buffer;
    db.bufferInfo.buffer = buffer->vkBuffer; // 使用之前修改的 Buffer::vkBuffer
    db.bufferInfo.offset = 0;
    db.bufferInfo.range = buffer->size;

    db.image = nullptr;
    db.imageInfo = {};

    bindings[binding].push_back(db);
}

void DescriptorSet::bindImageToDescriptorSet(uint32_t binding, VkDescriptorType type, VkShaderStageFlagBits stage, std::shared_ptr<Image> image) {
    const VkDescriptorSetLayoutBinding layoutBinding{
        .binding = binding,
        .descriptorType = type,
        .descriptorCount = 1,
        .stageFlags = stage
    };

    const auto& bindingVector = bindings[binding];
    if (!bindingVector.empty() && bindingVector[bindingVector.size()-1].layoutBinding.descriptorType != type) {
        LOGE("Binding already exists with different type");
    }
    
    DescriptorBinding db{};
    db.type = type;
    db.layoutBinding = layoutBinding;
    db.buffer = nullptr;
    db.bufferInfo = {};
    db.image = image;
    db.imageInfo = { {}, image->imageView, VK_IMAGE_LAYOUT_GENERAL };

    bindings[binding].push_back(db);
}

void DescriptorSet::build() {
    // 1. 创建 Layout
    std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
    for (auto& [_, options] : bindings) {
        layoutBindings.push_back(options[0].layoutBinding);
        if (options.size() > 1) {
            if (maxOptions == 1) maxOptions = options.size();
            else if (maxOptions != options.size()) {
                LOGE("Inconsistent number of alternative buffers");
            }
        }
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
    layoutInfo.pBindings = layoutBindings.data();

    if (vkCreateDescriptorSetLayout(context->device_, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        LOGE("Failed to create descriptor set layout");
    }

    // 2. 分配 Descriptor Sets
    uint32_t totalSets = framesInFlight * maxOptions;
    std::vector<VkDescriptorSetLayout> layouts(totalSets, descriptorSetLayout);
    
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = context->descriptorPool_; // 使用 Context.h 中的变量
    allocInfo.descriptorSetCount = totalSets;
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets.resize(totalSets);
    if (vkAllocateDescriptorSets(context->device_, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        LOGE("Failed to allocate descriptor sets");
    }

    // 3. 更新 Descriptor Sets (Write)
    for (int i = 0; i < framesInFlight; i++) {
        for (uint32_t j = 0; j < maxOptions; j++) {
            std::vector<VkWriteDescriptorSet> writes;
            // 必须在循环内保持 bufferInfo 的地址有效，所以先收集
            std::vector<VkDescriptorBufferInfo> bufferInfos; 
            bufferInfos.reserve(bindings.size());

            for (auto& binding : bindings) {
                uint32_t optIdx = (binding.second.size() == 1) ? 0 : j;
                auto& db = binding.second[optIdx];

                VkWriteDescriptorSet write{};
                write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.dstSet = descriptorSets[i * maxOptions + j];
                write.dstBinding = binding.first;
                write.dstArrayElement = 0;
                write.descriptorCount = 1;
                write.descriptorType = db.type;

                if (db.buffer != nullptr) {
                    db.buffer->boundToDescriptorSet(static_cast<std::weak_ptr<DescriptorSet>>(shared_from_this()), i * maxOptions + j, binding.first, db.type);
                    write.pBufferInfo = &db.bufferInfo;
                    write.pImageInfo = nullptr;
                } else {
                    write.pBufferInfo = nullptr;
                    write.pImageInfo = &db.imageInfo;
                }

                writes.push_back(write);
            }

            vkUpdateDescriptorSets(context->device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
    }
}

VkDescriptorSet DescriptorSet::getDescriptorSet(uint8_t currentFrame, uint8_t option) const {
    if (option >= maxOptions) {
        LOGE("Invalid option index");
    }
    return descriptorSets[currentFrame * maxOptions + option];
}
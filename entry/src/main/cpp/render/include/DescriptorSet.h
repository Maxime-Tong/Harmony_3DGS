//
// Created on 2026/4/8.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef HARMONYOS_3DGS_DESCRIPTORSET_H
#define HARMONYOS_3DGS_DESCRIPTORSET_H

#include <memory>
#include "Context.h"
#include "Buffer.h"
#include <unordered_map>

#include "Swapchain.h"

class DescriptorSet : public std::enable_shared_from_this<DescriptorSet> {
public:
    struct DescriptorBinding {
        VkDescriptorType type;
        VkDescriptorSetLayoutBinding layoutBinding;

        // buffer info
        std::shared_ptr<Buffer> buffer;
        VkDescriptorBufferInfo bufferInfo;

        // image info
        std::shared_ptr<Image> image; // 如果你有 Image 类
        VkDescriptorImageInfo imageInfo;
    };

    // 统一使用 Context 类名
    explicit DescriptorSet(const std::shared_ptr<Context> &context, uint8_t framesInFlight = 1);
    ~DescriptorSet(); // 需要析构函数来销毁 Layout

    void bindBufferToDescriptorSet(uint32_t binding, VkDescriptorType type, VkShaderStageFlagBits stage, std::shared_ptr<Buffer> buffer);
    void bindImageToDescriptorSet(uint32_t binding, VkDescriptorType type, VkShaderStageFlagBits stage, std::shared_ptr<Image> image);

    void build();

    VkDescriptorSet getDescriptorSet(uint8_t currentFrame, uint8_t option) const;

    // 如果需要绑定图像
    // void bindImageToDescriptorSet(uint32_t i, VkDescriptorType descriptor, VkShaderStageFlags stage, std::shared_ptr<Image> image);

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets;
    size_t maxOptions = 1;

private:
    const std::shared_ptr<Context> context;
    const uint8_t framesInFlight;
    std::unordered_map<uint32_t, std::vector<DescriptorBinding>> bindings;
};

#endif //HARMONYOS_3DGS_DESCRIPTORSET_H

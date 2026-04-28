#ifndef HARMONYOS_3DGS_PIPELINE_H
#define HARMONYOS_3DGS_PIPELINE_H

#include <memory>
#include <map>
#include <vector>
#include "Context.h"
#include "DescriptorSet.h"

class Pipeline {
public:
    struct DescriptorOption {
        bool multiple;
        uint32_t value;
        std::vector<uint32_t> values;
        DescriptorOption(uint32_t value) : multiple(false), value(value) {}
        DescriptorOption(std::vector<uint32_t> values) : multiple(true), values(std::move(values)) {}
        [[nodiscard]] uint32_t get(size_t index) const;
    };

    explicit Pipeline(const std::shared_ptr<Context>& context);
    virtual ~Pipeline(); // 必须有析构函数来释放原生句柄

    // 禁止拷贝
    Pipeline(const Pipeline &) = delete;
    Pipeline &operator=(const Pipeline &) = delete;

    void addDescriptorSet(uint32_t set, std::shared_ptr<DescriptorSet> descriptorSet);
    void addPushConstant(VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size);

    virtual void build() = 0;
    // 使用原生 VkCommandBuffer
    virtual void bind(VkCommandBuffer commandBuffer, uint8_t currentFrame, DescriptorOption option);

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;

protected:
    void buildPipelineLayout();

    std::shared_ptr<Context> context;
    std::vector<VkPushConstantRange> pushConstantRanges;
    std::map<uint32_t, std::shared_ptr<DescriptorSet>> descriptorSets;
};

#endif //VULKAN_SPLATTING_PIPELINE_H

//
// Created on 2026/4/7.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef HARMONYOS_3DGS_BUFFER_H
#define HARMONYOS_3DGS_BUFFER_H

#include <cstdint>
#include <memory>
#include <tuple>

//#include <vulkan/vulkan.h>
#include "Context.h"
#include "vk_mem_alloc.h"

class DescriptorSet;

class Buffer : public std::enable_shared_from_this<Buffer> {
public:
    Buffer(const std::shared_ptr<Context>& context, uint32_t size, VkBufferUsageFlags usage, VmaMemoryUsage vmaUsage,
           VmaAllocationCreateFlags flags, bool concurrentSharing = false, VkDeviceSize alignment = 0);

    Buffer(const Buffer &) = delete;

    Buffer(Buffer &&) = delete;

    Buffer &operator=(const Buffer &) = delete;

    Buffer &operator=(Buffer &&) = delete;

    ~Buffer();

    void realloc(uint64_t uint64);

    void boundToDescriptorSet(std::weak_ptr<DescriptorSet> descriptorSet, uint32_t set, uint32_t binding, VkDescriptorType type);

    static std::shared_ptr<Buffer> uniform(std::shared_ptr<Context> context, uint32_t size, bool concurrentSharing = false);

    static std::shared_ptr<Buffer> staging(std::shared_ptr<Context> context, uint32_t size);

    static std::shared_ptr<Buffer> storage(std::shared_ptr<Context> context, uint64_t size, bool concurrentSharing = false, VkDeviceSize alignment = 0);

    void upload(const void *data, uint32_t size, uint32_t offset = 0);

    void uploadFrom(std::shared_ptr<Buffer> buffer);

    std::vector<char> download();

    
    void downloadTo(std::shared_ptr<Buffer> buffer, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);

    void assertEquals(char *data, size_t length);

    template<typename T>
    T readOne(VkDeviceSize offset = 0) {
        if (vmaUsage == VMA_MEMORY_USAGE_GPU_ONLY || vmaUsage == VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE) {
            const auto stagingBuffer = Buffer::staging(context, sizeof(T));
            downloadTo(stagingBuffer, offset, 0);
            return *static_cast<T *>(stagingBuffer->allocation_info.pMappedData);
        } else if (flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) {
            return *(static_cast<T *>(allocation_info.pMappedData) + offset / sizeof(T));
        } else {
            throw std::runtime_error("Buffer is not mappable");
        }
    }

    void computeWriteReadBarrier(VkCommandBuffer commandBuffer);
    void computeReadWriteBarrier(VkCommandBuffer commandBuffer);
    void computeWriteWriteBarrier(VkCommandBuffer commandBuffer);

    VkDeviceSize size;
    VkBufferUsageFlags usage;
    uint64_t alignment;
    bool shared;

    VkBuffer vkBuffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocation_info;

    VmaMemoryUsage vmaUsage;
    VmaAllocationCreateFlags flags;


private:
    void alloc();

    Buffer createStagingBuffer(uint32_t size);
    std::shared_ptr<Context> context;

    std::vector<std::tuple<std::weak_ptr<DescriptorSet>, uint32_t, uint32_t, VkDescriptorType>> boundDescriptorSets;
};

class BarrierBuilder {
public:
    BarrierBuilder& queueFamilyIndex(uint32_t queueFamilyIndex);

    BarrierBuilder& addBufferBarrier(const std::shared_ptr<Buffer>&, VkAccessFlags srcAccessMask,
                                     VkAccessFlags dstAccessMask, uint32_t srcQueueFamilyIndex,
                                     uint32_t dstQueueFamilyIndex);

    BarrierBuilder& addBufferBarrier(const std::shared_ptr<Buffer>&, VkAccessFlags srcAccessMask,
                                     VkAccessFlags dstAccessMask);

    BarrierBuilder& srcQueueFamilyIndex(uint32_t srcQueueFamilyIndex);

    BarrierBuilder& dstQueueFamilyIndex(uint32_t dstQueueFamilyIndex);

    void build(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask) const;
private:
    std::vector<VkBufferMemoryBarrier> bufferMemoryBarriers;
    uint32_t _srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    uint32_t _dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
};


#endif //HARMONYOS_3DGS_BUFFER_H

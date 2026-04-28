//
// Created on 2026/4/7.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "Buffer.h"
#include "DescriptorSet.h"
#include "Context.h"

#include <cstdint>

#include <hilog/log.h>
#undef LOG_TAG
#define LOG_TAG "Buffer"

#undef LOG_DOMAIN
#define LOG_DOMAIN 0xFF04

#define LOGI(...) OH_LOG_INFO(LOG_APP, __VA_ARGS__)
#define LOGE(...) OH_LOG_ERROR(LOG_APP, __VA_ARGS__)

void Buffer::alloc() {
    // 1. 创建 Buffer 信息
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = (VkBufferUsageFlags)usage;
    bufferInfo.sharingMode = (shared ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE);
    
    if (shared) {
        uint32_t graphicsFamily = context->indices_.graphicsFamily;
        uint32_t computeFamily = context->indices_.computeFamily;
        uint32_t queueFamilyIndices[] = {graphicsFamily, computeFamily};
        
        bufferInfo.queueFamilyIndexCount = 2;
        bufferInfo.pQueueFamilyIndices = queueFamilyIndices;
    }

    // 2. 准备 VMA 分配信息
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = vmaUsage;
    allocInfo.flags = flags;

    // 3. 调用 VMA 创建
    VkResult res;
    if (alignment != 0) {
        res = vmaCreateBufferWithAlignment(context->allocator, &bufferInfo, &allocInfo, (VkDeviceSize)alignment, &vkBuffer,
                                           &allocation, &allocation_info);
    } else {
        res = vmaCreateBuffer(context->allocator, &bufferInfo, &allocInfo, &vkBuffer, &allocation, &allocation_info);
    }
    
    if (res != VK_SUCCESS) {
        LOGE("Failed to create Vulkan buffer, VkResult: %{public}d", res);
    }
}

Buffer::Buffer(const std::shared_ptr<Context>& _context, uint32_t _size, VkBufferUsageFlags _usage,
               VmaMemoryUsage _vmaUsage, VmaAllocationCreateFlags _flags, bool _shared, VkDeviceSize _alignment)
    : context(_context),
      size(_size),
      alignment(_alignment),
      shared(_shared),
      usage(_usage),
      vmaUsage(_vmaUsage),
      flags(_flags),
      allocation(nullptr){
    alloc();
}

Buffer Buffer::createStagingBuffer(uint32_t size) {
    return Buffer(context, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VMA_MEMORY_USAGE_AUTO,
                  VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, false);
}

void Buffer::upload(const void* data, uint32_t size, uint32_t offset) {
    if (size + offset > this->size) {
        throw std::runtime_error("Buffer overflow");
    }

    // 如果是 GPU 专用内存，需要通过临时 Staging Buffer 拷贝
    if (vmaUsage == VMA_MEMORY_USAGE_GPU_ONLY || vmaUsage == VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE) {
        auto stagingBuffer = createStagingBuffer(size);
        memcpy(stagingBuffer.allocation_info.pMappedData, ((const char *) data) + offset, size);
        
        // C 风格的 CommandBuffer 操作
        VkCommandBuffer cmd = context->beginOneTimeCommandBuffer();
        VkBufferCopy copyRegion = {};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = offset;
        copyRegion.size = size;
        
        vkCmdCopyBuffer(cmd, stagingBuffer.vkBuffer, this->vkBuffer, 1, &copyRegion);
        
        context->endOneTimeCommandBuffer(cmd, context->computeQueue_);
    } else if (flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) {
        // CPU 可见内存，直接 memcpy
        memcpy((char*)allocation_info.pMappedData + offset, data, size);
    } else {
        throw std::runtime_error("Buffer is not mappable");
    }
}

void Buffer::uploadFrom(std::shared_ptr<Buffer> otherBuffer) {
    if (otherBuffer->size > size) {
        throw std::runtime_error("Buffer overflow");
    }

    if (vmaUsage == VMA_MEMORY_USAGE_GPU_ONLY || vmaUsage == VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE) {
        VkCommandBuffer cmd = context->beginOneTimeCommandBuffer();
        VkBufferCopy copyRegion = {};
        copyRegion.size = otherBuffer->size;
        
        vkCmdCopyBuffer(cmd, otherBuffer->vkBuffer, this->vkBuffer, 1, &copyRegion);
        context->endOneTimeCommandBuffer(cmd, context->computeQueue_);
    } else if (flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) {
        memcpy(this->allocation_info.pMappedData, otherBuffer->allocation_info.pMappedData, otherBuffer->size);
    } else {
        throw std::runtime_error("Buffer is not mappable");
    }
}

void Buffer::downloadTo(std::shared_ptr<Buffer> otherBuffer, VkDeviceSize srcOffset, VkDeviceSize dstOffset) {
    if (vmaUsage == VMA_MEMORY_USAGE_GPU_ONLY || vmaUsage == VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE) {
        VkCommandBuffer cmd = context->beginOneTimeCommandBuffer();
        VkBufferCopy copyRegion = {};
        copyRegion.srcOffset = srcOffset;
        copyRegion.dstOffset = dstOffset;
        copyRegion.size = otherBuffer->size;
        
        vkCmdCopyBuffer(cmd, this->vkBuffer, otherBuffer->vkBuffer, 1, &copyRegion);
        context->endOneTimeCommandBuffer(cmd, context->computeQueue_);
    } else if (flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) {
        memcpy(otherBuffer->allocation_info.pMappedData, (char*)this->allocation_info.pMappedData + srcOffset, otherBuffer->size);
    } else {
        throw std::runtime_error("Buffer is not mappable");
    }
}

Buffer::~Buffer() {
    if (vkBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(context->allocator, vkBuffer, allocation);
        LOGI("Vulkan Buffer destroyed");
    }
}

void Buffer::realloc(uint64_t newSize) {
    vmaDestroyBuffer(context->allocator, vkBuffer, allocation);

    size = newSize;
    alloc();

    // 更新描述符信息
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = vkBuffer;
    bufferInfo.offset = allocation_info.offset;
    bufferInfo.range = size;

    std::vector<VkWriteDescriptorSet> writeSets;
    for (auto& tuple : boundDescriptorSets) {
        auto weakSet = std::get<0>(tuple);
        auto sharedSet = weakSet.lock();
        if (sharedSet) {
            VkWriteDescriptorSet write = {};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = sharedSet->descriptorSets[std::get<1>(tuple)]; // 假设这已经是 VkDescriptorSet
            write.dstBinding = std::get<2>(tuple);
            write.descriptorCount = 1;
            write.descriptorType = (VkDescriptorType)std::get<3>(tuple);
            write.pBufferInfo = &bufferInfo;
            writeSets.push_back(write);
        }
    }
    
    if (!writeSets.empty()) {
        vkUpdateDescriptorSets(context->device_, (uint32_t)writeSets.size(), writeSets.data(), 0, nullptr);
    }
}

void Buffer::boundToDescriptorSet(std::weak_ptr<DescriptorSet> descriptorSet, uint32_t set, uint32_t binding,
    VkDescriptorType type) {
    boundDescriptorSets.push_back({descriptorSet, set, binding, type});
}

// 静态辅助函数：Uniform Buffer
std::shared_ptr<Buffer> Buffer::uniform(std::shared_ptr<Context> context, uint32_t size, bool concurrentSharing) {
    return std::make_shared<Buffer>(context, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                    VMA_MEMORY_USAGE_AUTO,
                                    VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                                    concurrentSharing);
}

std::shared_ptr<Buffer> Buffer::staging(std::shared_ptr<Context> context, uint32_t size) {
    return std::make_shared<Buffer>(context, size,
                                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                    VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_MAPPED_BIT |
                                                           VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                                    false);
}

std::shared_ptr<Buffer> Buffer::storage(std::shared_ptr<Context> context, uint64_t size, bool concurrentSharing,
                                        VkDeviceSize alignment) {
    return std::make_shared<Buffer>(context, (uint32_t)size,
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                    VMA_MEMORY_USAGE_GPU_ONLY, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                                    concurrentSharing, alignment);
}

BarrierBuilder& BarrierBuilder::queueFamilyIndex(uint32_t queueFamilyIndex) {
    this->_srcQueueFamilyIndex = queueFamilyIndex;
    this->_dstQueueFamilyIndex = queueFamilyIndex;
    return *this;
}

BarrierBuilder& BarrierBuilder::addBufferBarrier(const std::shared_ptr<Buffer>&buffer,
                                                               const VkAccessFlags srcAccessMask,
                                                               const VkAccessFlags dstAccessMask,
                                                               const uint32_t srcQueueFamilyIndex,
                                                               const uint32_t dstQueueFamilyIndex) {
    bufferMemoryBarriers.push_back({
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = srcAccessMask,
        .dstAccessMask = dstAccessMask,
        .srcQueueFamilyIndex = srcQueueFamilyIndex,
        .dstQueueFamilyIndex = dstQueueFamilyIndex,
        .buffer = buffer->vkBuffer,
        .offset = 0,
        .size = buffer->size
    });
    return *this;
}
BarrierBuilder& BarrierBuilder::addBufferBarrier(const std::shared_ptr<Buffer>&buffer,
                                                               const VkAccessFlags srcAccessMask,
                                                               const VkAccessFlags dstAccessMask) {
    return addBufferBarrier(buffer, srcAccessMask, dstAccessMask, _srcQueueFamilyIndex,
                            _dstQueueFamilyIndex);
}

BarrierBuilder& BarrierBuilder::srcQueueFamilyIndex(uint32_t srcQueueFamilyIndex) {
    this->_srcQueueFamilyIndex = srcQueueFamilyIndex;
    return *this;
}

BarrierBuilder& BarrierBuilder::dstQueueFamilyIndex(uint32_t dstQueueFamilyIndex) {
    this->_dstQueueFamilyIndex = dstQueueFamilyIndex;
    return *this;
}

void BarrierBuilder::build(const VkCommandBuffer commandBuffer, const VkPipelineStageFlags srcStageMask,
                                  const VkPipelineStageFlags dstStageMask) const {
    vkCmdPipelineBarrier(
         commandBuffer,
         srcStageMask,
         dstStageMask,
         VkDependencyFlags(),
         0,
         nullptr,
         bufferMemoryBarriers.size(),
         bufferMemoryBarriers.data(),
         0,
         nullptr
    );
}

void Buffer::computeWriteReadBarrier(VkCommandBuffer commandBuffer) {
    BarrierBuilder().queueFamilyIndex(context->indices_.computeFamily)
            .addBufferBarrier(shared_from_this(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
            .build(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
}

void Buffer::computeReadWriteBarrier(VkCommandBuffer commandBuffer) {
    BarrierBuilder().queueFamilyIndex(context->indices_.computeFamily)
            .addBufferBarrier(shared_from_this(), VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT)
            .build(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
}

void Buffer::computeWriteWriteBarrier(VkCommandBuffer commandBuffer) {
    BarrierBuilder().queueFamilyIndex(context->indices_.computeFamily)
            .addBufferBarrier(shared_from_this(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT)
            .build(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
}


std::vector<char> Buffer::download() {
    auto stagingBuffer = Buffer::staging(context, size);
    downloadTo(stagingBuffer);
    
    char* mappedPtr = (char*)stagingBuffer->allocation_info.pMappedData;
    return std::vector<char>(mappedPtr, mappedPtr + size);
}




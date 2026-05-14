#include "Vulkan3DGS.h"
#include "json/json.h"
#include "shaders_half.h"
#include "Utils.h"

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <rawfile/raw_file_manager.h>
#include <native_window/external_window.h>

#include <hilog/log.h>

#undef LOG_TAG
#define LOG_TAG "Vulkan3DGS"

#undef LOG_DOMAIN
#define LOG_DOMAIN 0xFF00

#define LOGI(...) OH_LOG_INFO(LOG_APP, __VA_ARGS__)
#define LOGE(...) OH_LOG_ERROR(LOG_APP, __VA_ARGS__)

static bool inline CheckResult(VkResult result) {
    if (result != VK_SUCCESS) {
        LOGE("Fatal : VkResult is %{public}d", static_cast<int>(result));
        return false;
    }
    return true;
}

int inline highestBit(uint32_t x) {
    if (x & 0xFF000000) { return 32; }
    if (x & 0x00FF0000) { return 24; }
    if (x & 0x0000FF00) { return 16; }
    if (x & 0x000000FF) { return 8; }
    return 0;
}

uint32_t Vulkan3DGS::beginTimestamp(VkCommandBuffer commandBuffer, const char* label) {
    if (timestampQueryCursor_ + 1 >= kTimestampQueryCount) {
        LOGE("Timestamp query pool exhausted while starting %{public}s", label);
        return timestampQueryCursor_;
    }

    uint32_t startQuery = timestampQueryCursor_++;
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, context->queryPool_, startQuery);
    return startQuery;
}

void Vulkan3DGS::endTimestamp(VkCommandBuffer commandBuffer, std::vector<GpuTimingRange>& timings,
                              const char* label, uint32_t startQuery) {
    if (timestampQueryCursor_ >= kTimestampQueryCount) {
        LOGE("Timestamp query pool exhausted while ending %{public}s", label);
        return;
    }

    uint32_t endQuery = timestampQueryCursor_++;
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, context->queryPool_, endQuery);
    timings.push_back(GpuTimingRange{label, startQuery, endQuery});
}

void Vulkan3DGS::logTimingRanges(const char* phaseName, const std::vector<GpuTimingRange>& timings) const {
    for (const auto& timing : timings) {
        const double runtimeMs = context->GetTimestampDurationMs(timing.startQuery, timing.endQuery);
        if (runtimeMs >= 0.0) {
            LOGI("%{public}s - %{public}s runtime: %{public}.3f ms", phaseName, timing.label.c_str(), runtimeMs);
        } else {
            LOGI("%{public}s - %{public}s runtime: unavailable", phaseName, timing.label.c_str());
        }
    }
}

void Vulkan3DGS::setConfig(uint64_t width, uint64_t height, OHNativeWindow* window, NativeResourceManager* resourceManager) {
    config_.image_width = static_cast<int>(width);
    config_.image_height = static_cast<int>(height);
    config_.window_ = window;
    config_.resourceManager_ = resourceManager;
}

void Vulkan3DGS::initialize() {
    initializeVulkan();
    loadScene();
    createPreprocessPipeline();
    createPrefixSumPipeline();

    // createRadixSortPipeline();
    createXEngineSorter();
    
    createPreprocessSortPipeline();
    createTileBoundaryPipeline();
    createRenderPipeline();
    createCommandPool();
    recordPreprocessCommandBuffer();
}

void Vulkan3DGS::initializeVulkan() {
    LOGI("Initializing Vulkan");

    if (initialized_) { return; }
    initialized_ = true;

    context = std::make_shared<Context>(config_);
    
    context->CreateInstance();
    context->CreateSurface();
    context->PickPhysicalDevice();
    context->CreateLogicalDevice();
    
    swapchain = std::make_shared<Swapchain>(context);
    
    context->setupVma();
    context->CreateCommandPool();
    context->CreateSyncObjects();
    context->CreateDescriptorPool();
    context->CreateQueryPool();
}

void Vulkan3DGS::loadTestCameras() {
    // 读取文件内容
    std::vector<char> fileBuffer = VulkanUtils::readFile(config_.resourceManager_, config_.testCameras);
    std::string fileContent = std::string(fileBuffer.data());
    
    // 解析JSON
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::istringstream fileStream(fileContent);
    JSONCPP_STRING errs;
    
    if (!Json::parseFromStream(builder, fileStream, &root, &errs)) {
        LOGE("Error parsing camera file: %s", errs.c_str());
        return;
    }
    
    LOGI("Load %{public}d test cameras", root.size());
    
    // 处理相机数据
    for (auto& camera: root) {
        glm::vec3 pos = glm::vec3(
            camera["position"][0].asFloat(), 
            camera["position"][1].asFloat(), 
            camera["position"][2].asFloat());
        
        // column-major !
        glm::mat3 rot = glm::mat3(
            camera["rotation"][0][0].asFloat(), camera["rotation"][1][0].asFloat(), camera["rotation"][2][0].asFloat(), 
            camera["rotation"][0][1].asFloat(), camera["rotation"][1][1].asFloat(), camera["rotation"][2][1].asFloat(), 
            camera["rotation"][0][2].asFloat(), camera["rotation"][1][2].asFloat(), camera["rotation"][2][2].asFloat());
        
        glm::quat quat = glm::quat_cast(rot);
        float fov = 2.0 * std::atan2(camera["width"].asFloat(), 2.0 * camera["fx"].asFloat()) * (180.0 / 3.1415926535);
        
        testCameras.push_back(Camera{
            .position = pos,
            .rotation = quat,
            .fov = fov,
            .nearPlane = 0.1f,
            .farPlane = 1000.0f
        });
    }
}

void Vulkan3DGS::loadScene() {
    LOGI("Loading scene to GPU");
    scene = std::make_shared<GSScene>(config_.scene);
    scene->load(context);
    
    loadTestCameras();
    vkResetDescriptorPool(context->device_, context->descriptorPool_, 0);

    uint32_t tileX = (config_.image_width + 16 - 1) / 16;
    uint32_t tileY = (config_.image_height + 16 - 1) / 16;
    uint32_t numTiles = tileX * tileY;
    sortBufferSize = numTiles * 512;
}

void Vulkan3DGS::createPreprocessPipeline() {
    LOGI("Creating preprocess pipeline");
    uniformBuffer = Buffer::uniform(context, sizeof(UniformBuffer));
    vertexAttributeBuffer = Buffer::storage(context, scene->getNumVertices() * sizeof(VertexAttributeBuffer), false);
    prefixSumPingBuffer = Buffer::storage(context, scene->getNumVertices() * sizeof(uint32_t), false);
    // tileOverlapBuffer = Buffer::storage(context, scene->getNumVertices() * sizeof(uint32_t), false);

    preprocessPipeline = std::make_shared<ComputePipeline>(context, std::make_shared<Shader>(context, "preprocess", SPV_PREPROCESS, SPV_PREPROCESS_len));
    
    inputSet = std::make_shared<DescriptorSet>(context, FRAMES_IN_FLIGHT);
    inputSet->bindBufferToDescriptorSet(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT,
                                        scene->vertexBuffer);
    inputSet->bindBufferToDescriptorSet(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT,
                                        scene->cov3DBuffer);
    inputSet->build();
    preprocessPipeline->addDescriptorSet(0, inputSet);

    auto uniformOutputSet = std::make_shared<DescriptorSet>(context, FRAMES_IN_FLIGHT);
    uniformOutputSet->bindBufferToDescriptorSet(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                VK_SHADER_STAGE_COMPUTE_BIT,
                                                uniformBuffer);
    uniformOutputSet->bindBufferToDescriptorSet(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                VK_SHADER_STAGE_COMPUTE_BIT,
                                                vertexAttributeBuffer);
    uniformOutputSet->bindBufferToDescriptorSet(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                VK_SHADER_STAGE_COMPUTE_BIT,
                                                prefixSumPingBuffer);    
    uniformOutputSet->build();

    preprocessPipeline->addDescriptorSet(1, uniformOutputSet);
    preprocessPipeline->build();
}

void Vulkan3DGS::createPrefixSumPipeline() {
    LOGI("Creating prefix sum pipeline");
    // prefixSumPingBuffer = Buffer::storage(context, scene->getNumVertices() * sizeof(uint32_t), false);
    prefixSumPongBuffer = Buffer::storage(context, scene->getNumVertices() * sizeof(uint32_t), false);
    totalSumBufferHost = Buffer::staging(context, sizeof(uint32_t));

    prefixSumPipeline = std::make_shared<ComputePipeline>(context, std::make_shared<Shader>(context, "prefix_sum", SPV_PREFIX_SUM, SPV_PREFIX_SUM_len));
    auto descriptorSet = std::make_shared<DescriptorSet>(context, FRAMES_IN_FLIGHT);
    descriptorSet->bindBufferToDescriptorSet(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT,
                                             prefixSumPingBuffer);
    descriptorSet->bindBufferToDescriptorSet(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT,
                                             prefixSumPongBuffer);
    descriptorSet->build();

    prefixSumPipeline->addDescriptorSet(0, descriptorSet);
    prefixSumPipeline->addPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t));
    prefixSumPipeline->build();
}

void Vulkan3DGS::createXEngineSorter() {
    LOGI("Creating XEngineSorter");
    sorter = std::make_shared<XEngineSorter>(context);
    sortKeyBufferEven = Buffer::storage(context, sizeof(uint32_t) * sortBufferSize,
                                false, 0);
    sortValueBufferEven = Buffer::storage(context, sizeof(uint32_t) * sortBufferSize,
                                false, 0);
    sortCountBuffer = Buffer::storage(context, sizeof(uint32_t), false, 0);
}

void Vulkan3DGS::createPreprocessSortPipeline() {
    LOGI("Creating preprocess sort pipeline");
    preprocessSortPipeline = std::make_shared<ComputePipeline>(context, std::make_shared<Shader>(context, "preprocess_sort", SPV_PREPROCESS_SORT, SPV_PREPROCESS_SORT_len));
    
    auto descriptorSet = std::make_shared<DescriptorSet>(context, FRAMES_IN_FLIGHT);
    descriptorSet->bindBufferToDescriptorSet(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, vertexAttributeBuffer);
    descriptorSet->bindBufferToDescriptorSet(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, prefixSumPingBuffer);
    descriptorSet->bindBufferToDescriptorSet(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, prefixSumPongBuffer);
    descriptorSet->bindBufferToDescriptorSet(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, sortKeyBufferEven);
    descriptorSet->bindBufferToDescriptorSet(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, sortValueBufferEven);
    descriptorSet->build();

    preprocessSortPipeline->addDescriptorSet(0, descriptorSet);
    preprocessSortPipeline->addPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t));
    preprocessSortPipeline->build();
}

void Vulkan3DGS::createTileBoundaryPipeline() {
    LOGI("Creating tile boundary pipeline");
    uint32_t tileX = (config_.image_width + 16 - 1) / 16;
    uint32_t tileY = (config_.image_height + 16 - 1) / 16;
    tileBoundaryBuffer = Buffer::storage(context, tileX * tileY * sizeof(uint32_t) * 2, false);

    tileBoundaryPipeline = std::make_shared<ComputePipeline>(context, std::make_shared<Shader>(context, "tile_boundary", SPV_TILE_BOUNDARY, SPV_TILE_BOUNDARY_len));
    auto descriptorSet = std::make_shared<DescriptorSet>(context, FRAMES_IN_FLIGHT);
    descriptorSet->bindBufferToDescriptorSet(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, sortKeyBufferEven);
//    descriptorSet->bindBufferToDescriptorSet(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, sortKeyBufferOdd);
    descriptorSet->bindBufferToDescriptorSet(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, tileBoundaryBuffer);
    descriptorSet->build();

    tileBoundaryPipeline->addDescriptorSet(0, descriptorSet);
    tileBoundaryPipeline->addPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t));
    tileBoundaryPipeline->build();
}

void Vulkan3DGS::createRenderPipeline() {
    LOGI("Creating render pipeline");
    renderPipeline = std::make_shared<ComputePipeline>(context, std::make_shared<Shader>(context, "render", SPV_RENDER, SPV_RENDER_len));
    
    auto inputSet = std::make_shared<DescriptorSet>(context, FRAMES_IN_FLIGHT);
    inputSet->bindBufferToDescriptorSet(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, vertexAttributeBuffer);
    inputSet->bindBufferToDescriptorSet(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, tileBoundaryBuffer);
    inputSet->bindBufferToDescriptorSet(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, sortValueBufferEven);
    inputSet->build();

    auto outputSet = std::make_shared<DescriptorSet>(context, FRAMES_IN_FLIGHT);
    for (auto& image : swapchain->swapchainImages) {
        outputSet->bindImageToDescriptorSet(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, image);
    }
    outputSet->build();

    renderPipeline->addDescriptorSet(0, inputSet);
    renderPipeline->addDescriptorSet(1, outputSet);
    renderPipeline->addPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t) * 2);
    renderPipeline->build();
}

void Vulkan3DGS::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = context->indices_.computeFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(context->device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS) {
        LOGE("Failed to create command pool");
    }
}

void Vulkan3DGS::recordPreprocessCommandBuffer() {
    LOGI("Recording preprocess command buffer");

    if (!preprocessCommandBuffer_) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool_;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        vkAllocateCommandBuffers(context->device_, &allocInfo, &preprocessCommandBuffer_);
    }

    vkResetCommandBuffer(preprocessCommandBuffer_, 0);
    preprocessTimings_.clear();
    VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(preprocessCommandBuffer_, &beginInfo);

    // Reset the timestamp pool before recording the frame so query slots are valid when reused.
    context->ResetTimestampQueryPool(preprocessCommandBuffer_, kTimestampQueryCount);

    uint32_t numVertices = scene->getNumVertices();
    uint32_t numGroups = (numVertices + 255) / 256;
    
    // Preprocess
    uint32_t preprocessStart = beginTimestamp(preprocessCommandBuffer_, "preprocess");
    preprocessPipeline->bind(preprocessCommandBuffer_, 0, 0);
    vkCmdDispatch(preprocessCommandBuffer_, numGroups, 1, 1);
    endTimestamp(preprocessCommandBuffer_, preprocessTimings_, "preprocess", preprocessStart);
    
    // // 2. Copy to PrefixSum Buffer
    // tileOverlapBuffer->computeWriteReadBarrier(preprocessCommandBuffer_);
    // VkBufferCopy copyRegion{0, 0,  tileOverlapBuffer->size};
    // vkCmdCopyBuffer(preprocessCommandBuffer_, tileOverlapBuffer->vkBuffer, prefixSumPingBuffer->vkBuffer, 1, &copyRegion);

    prefixSumPingBuffer->computeWriteReadBarrier(preprocessCommandBuffer_);

    // 3. Prefix Sum Iterations
    uint32_t prefixSumStart = beginTimestamp(preprocessCommandBuffer_, "prefix_sum");
    prefixSumPipeline->bind(preprocessCommandBuffer_, 0, 0);
    uint32_t iters = static_cast<uint32_t>(std::ceil(std::log2(static_cast<float>(scene->getNumVertices()))));
    LOGI("Prefix sum iterations: %{public}d", iters);

    for (uint32_t timestep = 0; timestep < iters; timestep++) {
        vkCmdPushConstants(preprocessCommandBuffer_, prefixSumPipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &timestep);
        vkCmdDispatch(preprocessCommandBuffer_, numGroups, 1, 1);
        
        if (timestep % 2 == 0) {
            prefixSumPongBuffer->computeWriteReadBarrier(preprocessCommandBuffer_);
            prefixSumPingBuffer->computeReadWriteBarrier(preprocessCommandBuffer_);
        } else {
            prefixSumPingBuffer->computeWriteReadBarrier(preprocessCommandBuffer_);
            prefixSumPongBuffer->computeReadWriteBarrier(preprocessCommandBuffer_);
        }
    }

    endTimestamp(preprocessCommandBuffer_, preprocessTimings_, "prefix_sum", prefixSumStart);

    VkBufferCopy totalSumRegion = { 
        .srcOffset = (numVertices - 1) * sizeof(uint32_t),
        .dstOffset = 0,
        .size = sizeof(uint32_t)
    };

    if (iters % 2 == 0) {
        vkCmdCopyBuffer(preprocessCommandBuffer_, prefixSumPingBuffer->vkBuffer, totalSumBufferHost->vkBuffer, 1, &totalSumRegion);
        vkCmdCopyBuffer(preprocessCommandBuffer_, prefixSumPingBuffer->vkBuffer, sortCountBuffer->vkBuffer, 1, &totalSumRegion);
    } else {
        vkCmdCopyBuffer(preprocessCommandBuffer_, prefixSumPongBuffer->vkBuffer, totalSumBufferHost->vkBuffer, 1, &totalSumRegion);
        vkCmdCopyBuffer(preprocessCommandBuffer_, prefixSumPongBuffer->vkBuffer, sortCountBuffer->vkBuffer, 1, &totalSumRegion);
    }

    vkEndCommandBuffer(preprocessCommandBuffer_);
}

bool Vulkan3DGS::recordRenderCommandBuffer(uint32_t currentFrame) {
    // LOGI("Recording render command buffer for frame %{public}d", currentFrame);

    if (renderCommandBuffers_.empty()) {
        renderCommandBuffers_.resize(FRAMES_IN_FLIGHT, VK_NULL_HANDLE);
        VkCommandBufferAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = commandPool_,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = FRAMES_IN_FLIGHT
        };
        vkAllocateCommandBuffers(context->device_, &allocInfo, renderCommandBuffers_.data());
    }

    VkCommandBuffer renderCommandBuffer = renderCommandBuffers_[currentFrame];

    uint32_t numInstances = totalSumBufferHost->readOne<uint32_t>();
    
    if (numInstances > sortBufferSize) {
        auto old = sortBufferSize;
        
        uint32_t sortBufferSizeMultiplier = 1;
        while (numInstances > sortBufferSize * sortBufferSizeMultiplier) {
            sortBufferSizeMultiplier++;
        }
        sortBufferSize *= sortBufferSizeMultiplier;

        LOGI("Reallocating sort buffers. %{public}d -> %{public}d", old, sortBufferSize);
        sortKeyBufferEven->realloc(sizeof(uint32_t) * sortBufferSize);
        sortValueBufferEven->realloc(sizeof(uint32_t) * sortBufferSize);
        recordPreprocessCommandBuffer();
        return false;
    }

    vkResetCommandBuffer(renderCommandBuffer, 0);
    renderTimings_.clear();
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = 0
    };
    vkBeginCommandBuffer(renderCommandBuffer, &beginInfo);

    vertexAttributeBuffer->computeWriteReadBarrier(renderCommandBuffer);

    // ================== preprocess sort =========================
    const auto iters = static_cast<uint32_t>(std::ceil(std::log2(static_cast<float>(scene->getNumVertices()))));
    auto numGroups = (scene->getNumVertices() + 255) / 256;
    preprocessSortPipeline->bind(renderCommandBuffer, currentFrame, iters % 2 == 0 ? 0 : 1);
    
//    renderCommandBuffer_->writeTimestamp(vk::PipelineStageFlagBits::eComputeShader, context->queryPool.get(),
//                                            queryManager->registerQuery("preprocess_sort_start"));
    uint32_t tileX = (swapchain->swapchainExtent.width + 16 - 1) / 16;
    uint32_t preprocessSortStart = beginTimestamp(renderCommandBuffer, "preprocess_sort");
    vkCmdPushConstants(renderCommandBuffer, 
                       preprocessSortPipeline->pipelineLayout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 
                       0, 
                       sizeof(uint32_t), 
                       &tileX);
    vkCmdDispatch(renderCommandBuffer, numGroups, 1, 1);
    endTimestamp(renderCommandBuffer, renderTimings_, "preprocess_sort", preprocessSortStart);

    sortValueBufferEven->computeWriteReadBarrier(renderCommandBuffer);
//    assert(numInstances <= scene->getNumVertices() * sortBufferSizeMultiplier);
    
    // ================== sort =========================
    uint32_t sortStart = beginTimestamp(renderCommandBuffer, "sort");
    sorter->cmdDispatchSort(renderCommandBuffer, sortKeyBufferEven, sortValueBufferEven, sortCountBuffer);
    endTimestamp(renderCommandBuffer, renderTimings_, "sort", sortStart);
    // ================== tile boundary =========================
    vkCmdFillBuffer(renderCommandBuffer, 
                tileBoundaryBuffer->vkBuffer, 
                0, 
                VK_WHOLE_SIZE, 
                0);

    BarrierBuilder()
        .queueFamilyIndex(context->indices_.computeFamily)
        .addBufferBarrier(tileBoundaryBuffer, 
                          VK_ACCESS_TRANSFER_WRITE_BIT,
                          VK_ACCESS_SHADER_WRITE_BIT)
        .build(renderCommandBuffer, 
               VK_PIPELINE_STAGE_TRANSFER_BIT,
               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    
    tileBoundaryPipeline->bind(renderCommandBuffer, currentFrame, 0);

    uint32_t tileBoundaryStart = beginTimestamp(renderCommandBuffer, "tile_boundary");
    vkCmdPushConstants(renderCommandBuffer, 
                       tileBoundaryPipeline->pipelineLayout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 
                       0, 
                       sizeof(uint32_t), 
                       &numInstances);
    vkCmdDispatch(renderCommandBuffer, (numInstances + 255) / 256, 1, 1);
    endTimestamp(renderCommandBuffer, renderTimings_, "tile_boundary", tileBoundaryStart);

    // ======================== render ==============================
    std::vector<uint32_t> descriptorSets = {0, currentImageIndex};
    renderPipeline->bind(renderCommandBuffer, currentFrame, descriptorSets);
    
    auto [width, height] = swapchain->swapchainExtent;
    uint32_t constants[2] = {width, height};
    
    vkCmdPushConstants(renderCommandBuffer, 
                       renderPipeline->pipelineLayout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 
                       0, 
                       sizeof(uint32_t) * 2, 
                       constants);

    // image layout transition: undefined -> general
    VkImageMemoryBarrier imageMemoryBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_NONE,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = swapchain->swapchainImages[currentImageIndex]->image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    vkCmdPipelineBarrier(renderCommandBuffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_DEPENDENCY_BY_REGION_BIT,
                         0, nullptr,
                         0, nullptr,
                         1, &imageMemoryBarrier);

    uint32_t renderStart = beginTimestamp(renderCommandBuffer, "render");
    vkCmdDispatch(renderCommandBuffer, (width + 15) / 16, (height + 15) / 16, 1);
    endTimestamp(renderCommandBuffer, renderTimings_, "render", renderStart);

    // image layout transition: general -> present
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    
    VkImageMemoryBarrier imageMemoryBarrierGenToPre = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = 0,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = swapchain->swapchainImages[currentImageIndex]->image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    
    vkCmdPipelineBarrier(renderCommandBuffer,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         VK_DEPENDENCY_BY_REGION_BIT,
                         0, nullptr, 0, nullptr, 1, &imageMemoryBarrierGenToPre);

    vkEndCommandBuffer(renderCommandBuffer);

    return true;
}

void Vulkan3DGS::recreateSwapchain() {
    auto oldExtent = swapchain->swapchainExtent;
    LOGI("Recreating swapchain");
    
    swapchain->recreate();
    
    auto [width, height] = swapchain->swapchainExtent;
    auto [oldWidth, oldHeight] = oldExtent;
    if (width == oldWidth && height == oldHeight) {
        return;
    }

    auto tileX = (width + 16 - 1) / 16;
    auto tileY = (height + 16 - 1) / 16;
    tileBoundaryBuffer->realloc(tileX * tileY * sizeof(uint32_t) * 2);

    recordPreprocessCommandBuffer();
    createRenderPipeline();
}

void Vulkan3DGS::updateUniforms() {
    UniformBuffer data{};
    auto [width, height] = swapchain->swapchainExtent;
    data.width = width;
    data.height = height;
    data.camera_position = glm::vec4(camera.position, 1.0f);    

    auto rotation = glm::mat4_cast(camera.rotation);
    auto translation = glm::translate(glm::mat4(1.0f), camera.position);
    auto view = glm::inverse(translation * rotation);

    float tan_fovx = std::tan(glm::radians(camera.fov) / 2.0);
    float tan_fovy = tan_fovx * static_cast<float>(height) / static_cast<float>(width);
    data.view_mat = view;
    data.proj_mat = glm::perspective(std::atan(tan_fovy) * 2.0f,
                                     static_cast<float>(width) / static_cast<float>(height),
                                     camera.nearPlane,
                                     camera.farPlane) * view;

    data.view_mat[0][1] *= -1.0f;
    data.view_mat[1][1] *= -1.0f;
    data.view_mat[2][1] *= -1.0f;
    data.view_mat[3][1] *= -1.0f;

    data.proj_mat[0][1] *= -1.0f;
    data.proj_mat[1][1] *= -1.0f;
    data.proj_mat[2][1] *= -1.0f;
    data.proj_mat[3][1] *= -1.0f;

    data.tan_fovx = tan_fovx;
    data.tan_fovy = tan_fovy;
    uniformBuffer->upload(&data, sizeof(UniformBuffer), 0);
}

void Vulkan3DGS::draw() {
    CheckResult(vkWaitForFences(context->device_, 1, &context->inFlightFences_[currentFrameIndex], VK_TRUE, UINT64_MAX));
    CheckResult(vkResetFences(context->device_, 1, &context->inFlightFences_[currentFrameIndex]));

    // Reset timestamp cursor for this new frame so recorded queries use fresh slots
    timestampQueryCursor_ = 0;

    auto res = vkAcquireNextImageKHR(context->device_, swapchain->vkSwapchain, UINT64_MAX,
                          swapchain->imageAvailableSemaphores[currentFrameIndex],
                          nullptr, &currentImageIndex);

    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    } else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        LOGE("Failed to acquire swapchain image");
    }

    camera = testCameras[testCameraIndex];
    updateUniforms();

    int nextCameraIndex = testCameraIndex + direction;
    if (nextCameraIndex >= testCameras.size() || nextCameraIndex < 0) {
        direction = -direction;
    }
    testCameraIndex += direction;

    do {
        VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &preprocessCommandBuffer_
        };
        
        CheckResult(vkQueueSubmit(context->computeQueue_, 1, &submitInfo, context->inFlightFences_[currentFrameIndex]));
        CheckResult(vkWaitForFences(context->device_, 1, &context->inFlightFences_[currentFrameIndex], VK_TRUE, UINT64_MAX));
        CheckResult(vkResetFences(context->device_, 1, &context->inFlightFences_[currentFrameIndex]));
        
    } while (!recordRenderCommandBuffer(currentFrameIndex));
        
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    VkSubmitInfo renderSubmitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &swapchain->imageAvailableSemaphores[currentFrameIndex], // 等待图像就绪
        .pWaitDstStageMask = &waitStage,
        .commandBufferCount = 1,
        .pCommandBuffers = &renderCommandBuffers_[currentFrameIndex],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &context->renderFinishedSemaphores_[currentFrameIndex] // 信号：渲染完成
    };
    CheckResult(vkQueueSubmit(context->computeQueue_, 1, &renderSubmitInfo, context->inFlightFences_[currentFrameIndex]));
    
    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &context->renderFinishedSemaphores_[currentFrameIndex],
        .swapchainCount = 1,
        .pSwapchains = &swapchain->vkSwapchain,
        .pImageIndices = &currentImageIndex
    };
    auto ret = vkQueuePresentKHR(context->presentQueue_, &presentInfo);
    if (ret == VK_ERROR_OUT_OF_DATE_KHR || ret == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
    }

    // Current frame is complete enough to read back and log the timings we just recorded.
    CheckResult(vkWaitForFences(context->device_, 1, &context->inFlightFences_[currentFrameIndex], VK_TRUE, UINT64_MAX));
    if (!preprocessTimings_.empty()) {
        logTimingRanges("preprocess", preprocessTimings_);
        // preprocessTimings_.clear();
    }
    if (!renderTimings_.empty()) {
        logTimingRanges("render", renderTimings_);
        // renderTimings_.clear();
    }

    currentFrameIndex = (currentFrameIndex + 1) % FRAMES_IN_FLIGHT;
}

void Vulkan3DGS::waitDeviceIde() {
    vkDeviceWaitIdle(context->device_);
}

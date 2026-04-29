#include "Context.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <hilog/log.h>
#include <set>
#include <vector>

#undef LOG_TAG
#define LOG_TAG "VulkanContext"

#undef LOG_DOMAIN
#define LOG_DOMAIN 0xFF01

#define LOGI(...) OH_LOG_INFO(LOG_APP, __VA_ARGS__)
#define LOGE(...) OH_LOG_ERROR(LOG_APP, __VA_ARGS__)

static bool inline CheckResult(VkResult result) {
    if (result != VK_SUCCESS) {
        LOGE("Fatal: VkResult is %{public}d", static_cast<int>(result));
        return false;
    }
    return true;
}

Context::Context(RendererConfiguration config) : config_(config) {}

Context::~Context() {
    if (allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(allocator);
    }
}

//void Renderer::initialize() {
//    initializeVulkan();
//}

//void Renderer::initializeVulkan() {
//    CreateInstance();
//    vkExample::utils::LoadVulkanFunctions(instance_);
//    CreateSurface();
//    PickPhysicalDevice();
//    CreateLogicalDevice();
//    vkExample::utils::LoadVulkanFunctions(device_);
//    
//    CreateCommandPool();
//    CreateSyncObjects();
//    CreateDescriptorPool();
//    CreateQueryPool();
//}

bool Context::CreateInstance() {
    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, 
        .pApplicationName = "Vulkan3DGS",
        .pEngineName = "vulkanExample",
        .apiVersion = VK_API_VERSION_1_3
    };
    
    VkInstanceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size()),
        .ppEnabledExtensionNames = instanceExtensions.data()
    };
    bool res = CheckResult(vkCreateInstance(&createInfo, nullptr, &instance_));
    
    return res;
}

bool Context::CreateSurface() {
    VkSurfaceCreateInfoOHOS surfaceCreateInfo{};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_SURFACE_CREATE_INFO_OHOS;
    if (config_.window_ == nullptr) {
        LOGE("Failed to create surface !");
        return false;
    }
    surfaceCreateInfo.window = config_.window_;
    surfaceCreateInfo.flags = 0;
    surfaceCreateInfo.pNext = nullptr;
    bool res = CheckResult(vkCreateSurfaceOHOS(instance_, &surfaceCreateInfo, nullptr, &surface_));
    return res;
}

bool Context::PickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);

    if (deviceCount == 0) {
        LOGE("Failed to find GPUs with Vulkan support!");
        return false;
    }
    
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());
    for (const auto &device : devices) {
        indices_ = FindQueueFamilies(device);
        if (IsDeviceSuitable(device)) {
            physicalDevice_ = device;
            break;
        }
    }
    
    if (physicalDevice_ == VK_NULL_HANDLE) {
        LOGE("No suitable GPU found!");
        return false;
    }
    return true;
}

bool Context::CreateLogicalDevice() {
    indices_ = FindQueueFamilies(physicalDevice_);

    std::set<uint32_t> uniqueQueueFamilies;
    uniqueQueueFamilies.insert(static_cast<uint32_t>(indices_.graphicsFamily));
    uniqueQueueFamilies.insert(static_cast<uint32_t>(indices_.presentFamily));
    uniqueQueueFamilies.insert(static_cast<uint32_t>(indices_.computeFamily));
    
    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    
    VkPhysicalDeviceVulkan12Features vulkan12Features{};
    vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &vulkan12Features;
    features2.features = deviceFeatures;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &features2;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    
    bool res = CheckResult(vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_));
    if (!res) return false;

    vkGetDeviceQueue(device_, indices_.graphicsFamily, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, indices_.presentFamily, 0, &presentQueue_);
    vkGetDeviceQueue(device_, indices_.computeFamily, 0, &computeQueue_);
    
    LOGI("Queues acquired - Graphics: %{public}p, Present: %{public}p, Compute: %{public}p", 
         graphicsQueue_, presentQueue_, computeQueue_);
    
    return true;
}

// This suitable GPU should support graphics & present queue family and
// extension 'VK_KHR_SWAPCHAIN_EXTENSION_NAME'.
bool Context::IsDeviceSuitable(VkPhysicalDevice device) {
    // 获取设备属性用于日志
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    
    QueueFamilyIndices indices = FindQueueFamilies(device);
    if (!indices.IsComplete()) {
        LOGE("Device %{public}s: QueueFamilyIndices incomplete", deviceProperties.deviceName);
        return false;
    }
    
    bool extensionsSupported = CheckDeviceExtensionSupport(device);
    if (!extensionsSupported) {
        LOGE("Device %{public}s: Extensions not supported", deviceProperties.deviceName);
        return false;
    }
    
    bool swapChainAdequate = false;
    SwapChainSupportDetails swapchainSupport = QuerySwapChainSupport(device);
    swapChainAdequate = !swapchainSupport.formats.empty() && !swapchainSupport.presentModes.empty();
    
    if (!swapChainAdequate) {
        LOGE("Device %s: SwapChain inadequate (formats: %zu, presentModes: %zu)", 
                     deviceProperties.deviceName, 
                     swapchainSupport.formats.size(), 
                     swapchainSupport.presentModes.size());
        return false;
    }
    
//    // 检查 feature 特性支持
//    VkPhysicalDeviceFeatures supportedFeatures;
//    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);
//    
//    bool featuresSupported = supportedFeatures.shaderStorageImageWriteWithoutFormat &&
//                             supportedFeatures.shaderInt64;
//    
//    if (!featuresSupported) {
//        LOGE("Device %{public}s: Features not supported (shaderStorageImageWriteWithoutFormat: %{public}d, shaderInt64: %{public}d)", 
//                     deviceProperties.deviceName,
//                     supportedFeatures.shaderStorageImageWriteWithoutFormat,
//                     supportedFeatures.shaderInt64);
//        return false;
//    }
//
//    VkPhysicalDeviceVulkan12Features vulkan12Features{};
//    vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
//    
//    VkPhysicalDeviceFeatures2 features2{};
//    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
//    features2.pNext = &vulkan12Features;
//    
//    vkGetPhysicalDeviceFeatures2(device, &features2);
//    
//    bool vulkan12Supported = vulkan12Features.shaderBufferInt64Atomics &&
//                             vulkan12Features.shaderSharedInt64Atomics;
//    
//    if (!vulkan12Supported) {
//        LOGE("Device %s: Vulkan 1.2 features not supported (shaderBufferInt64Atomics: %d, shaderSharedInt64Atomics: %d)", 
//                     deviceProperties.deviceName,
//                     vulkan12Features.shaderBufferInt64Atomics,
//                     vulkan12Features.shaderSharedInt64Atomics);
//        return false;
//    }
    
    LOGI("Device %s: All conditions satisfied, device is suitable", deviceProperties.deviceName);
    return true;
}

// Find graphics family index and present family index of the physical device.
QueueFamilyIndices Context::FindQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto &queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }
        
        if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            indices.computeFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.IsComplete()) {
            break;
        }
        i++;
    }

    return indices;
}

// Check whether the physical device supports all required extensions.
bool Context::CheckDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto &extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

SwapChainSupportDetails Context::QuerySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &details.capabilities);
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, details.presentModes.data());
    }

    return details;
}

bool Context::setupVma() {
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = physicalDevice_;
    allocatorInfo.device = device_;
    allocatorInfo.instance = instance_;
    // allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &allocator);
    return true;
}

bool Context::CreateCommandPool() {
    // 创建计算专用的命令池
    // 注意：即使Graphics和Compute是同一队列族，3DGS也主要使用Compute
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = indices_.computeFamily;  // 使用Compute队列族
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;  // 允许重置单个命令缓冲
    
    bool res = CheckResult(vkCreateCommandPool(device_, &poolInfo, nullptr, &computeCommandPool_));
    if (!res) return false;
    
    // 分配初始命令缓冲（后续每帧可能使用多个，这里先分配一个示例）
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = computeCommandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    
    res = CheckResult(vkAllocateCommandBuffers(device_, &allocInfo, &computeCommandBuffer_));
    
    LOGI("Compute command pool created (family: %d)", indices_.computeFamily);
    return res;
}

bool Context::CreateSyncObjects() {
    inFlightFences_.resize(FRAMES_IN_FLIGHT);
    renderFinishedSemaphores_.resize(FRAMES_IN_FLIGHT);
    
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // 初始为已信号状态，避免首次wait阻塞
    
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
        bool res = CheckResult(vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFences_[i]));
        if (!res) return false;
        
        res = CheckResult(vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i]));
        if (!res) return false;
    }
    
    LOGI("Sync objects created (%d frames in flight)", FRAMES_IN_FLIGHT);
    return true;
}

bool Context::CreateDescriptorPool() {
    // 3DGS需要大量描述符：SSBO（Storage Buffer）用于点云数据、排序缓冲等
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100},   // 存储缓冲（主要）
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10},    // 统一缓冲（相机参数）
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10}     // 存储图像（Swapchain写入）
//        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10}  // 纹理采样（如果有）
    };
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 100;  // 最大描述符集数量
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;  // 允许单独释放
    
    bool res = CheckResult(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_));
    LOGI("Descriptor pool created");
    return res;
}

bool Context::CreateQueryPool() {
    // GPU时间戳查询池，用于性能分析（3DGS各阶段计时）
    // 参照原实现：preprocess, prefix_sum, sort, tile_boundary, render 等阶段
    constexpr uint32_t QUERY_COUNT = 12;  // 6对开始/结束时间戳
    
    VkQueryPoolCreateInfo queryPoolInfo{};
    queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryPoolInfo.queryCount = QUERY_COUNT;
    
    if (!CheckResult(vkCreateQueryPool(device_, &queryPoolInfo, nullptr, &queryPool_))) {
        return false;
    }
    
    VkCommandBuffer cmd = beginOneTimeCommandBuffer();
    if (cmd == VK_NULL_HANDLE) {
        return false;
    }

    vkCmdResetQueryPool(cmd, queryPool_, 0, QUERY_COUNT);

    endOneTimeCommandBuffer(cmd, computeQueue_); 
    LOGI("Query pool created and reset (%d queries)", QUERY_COUNT);
    return true;
}


VkCommandBuffer Context::beginOneTimeCommandBuffer() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = computeCommandPool_; // 使用现有的计算命令池
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        LOGE("Failed to allocate one-time command buffer");
        return VK_NULL_HANDLE;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // 提交后即失效

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        LOGE("Failed to begin recording one-time command buffer");
        return VK_NULL_HANDLE;
    }

    return commandBuffer;
}

void Context::endOneTimeCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue) {
    if (commandBuffer == VK_NULL_HANDLE) return;

    // 1. 结束录制
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        LOGE("Failed to end command buffer recording");
    }

    // 2. 提交任务到队列
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    if (vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        LOGE("Failed to submit one-time command buffer to queue");
    }

    // 3. 阻塞等待（确保数据拷贝完成）
    // 注意：在高性能循环中通常使用 Fence，但在 Buffer 上传场景下 WaitIdle 是最稳妥的实现
    vkQueueWaitIdle(queue);

    // 4. 立即释放临时缓冲
    vkFreeCommandBuffers(device_, computeCommandPool_, 1, &commandBuffer);
}
#ifndef HARMONYOS_3DGS_RENDERER_H
#define HARMONYOS_3DGS_RENDERER_H

#include "vk_mem_alloc.h"

#include <string>
#include <vulkan/vulkan.h>
#include <rawfile/raw_file_manager.h>

#define FRAMES_IN_FLIGHT 1

struct RendererConfiguration {
    float fov = 45.0f;
    float near = 0.2f;
    float far = 1000.0f;
    std::string scene = "models/plant_whitepalm/point_cloud.ply";
    std::string testCameras = "models/plant_whitepalm/cameras.json";
    
    int image_height = 720;
    int image_width = 1280;
    
    bool immediateSwapchain = false;

    OHNativeWindow* window_;
    NativeResourceManager* resourceManager_;
};

struct QueueFamilyIndices {
    int graphicsFamily = -1;
    int presentFamily = -1;
    int computeFamily = -1;
    bool IsComplete() { return graphicsFamily >= 0 && presentFamily >= 0 && computeFamily >= 0; }
    bool CanMergeGraphicsCompute() { return graphicsFamily == computeFamily; }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class Context {
public:
    explicit Context(RendererConfiguration config);
    virtual ~Context();
    
    // Initialize Vulkan functions
    bool CreateInstance();
    bool CreateSurface();
    bool PickPhysicalDevice();
    bool CreateLogicalDevice();

    bool setupVma();
    bool CreateCommandPool();           // 创建计算专用命令池
    bool CreateSyncObjects();           // 创建Fence和Semaphore
    bool CreateDescriptorPool();        // 创建描述符池
    bool CreateQueryPool();             // 创建查询池（GPU计时）
    void ResetTimestampQueryPool(VkCommandBuffer commandBuffer, uint32_t queryCount);
    double GetTimestampDurationMs(uint32_t startQuery, uint32_t endQuery) const;
    
    // command buffer
    VkCommandBuffer beginOneTimeCommandBuffer();
    void endOneTimeCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue);
    
    // Util functions
    bool IsDeviceSuitable(VkPhysicalDevice device);
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
    bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);

    // Variables
    RendererConfiguration config_;
    
    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    VkQueue computeQueue_ = VK_NULL_HANDLE;
    QueueFamilyIndices indices_;

    VkCommandPool computeCommandPool_ = VK_NULL_HANDLE;
    VkCommandBuffer computeCommandBuffer_ = VK_NULL_HANDLE;  // 当前帧命令缓冲
    
    std::vector<VkFence> inFlightFences_;           // CPU等待GPU完成
    std::vector<VkSemaphore> renderFinishedSemaphores_;  // GPU信号量
    
    VmaAllocator allocator = VK_NULL_HANDLE;
    
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkQueryPool queryPool_ = VK_NULL_HANDLE;
    float timestampPeriod_ = 0.0f;
    
    std::vector<const char*> instanceExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_OHOS_SURFACE_EXTENSION_NAME
    };
    const std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
};


#endif // HARMONYOS_3DGS_RENDERER_H

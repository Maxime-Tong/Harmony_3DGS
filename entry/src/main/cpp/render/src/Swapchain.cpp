#include "Swapchain.h"
#include "Context.h"

#include <algorithm>

#include <hilog/log.h>

#undef LOG_TAG
#define LOG_TAG "Swapchain"

#undef LOG_DOMAIN
#define LOG_DOMAIN 0xFF02

#define LOGI(...) OH_LOG_INFO(LOG_APP, __VA_ARGS__)
#define LOGE(...) OH_LOG_ERROR(LOG_APP, __VA_ARGS__)

Swapchain::Swapchain(std::shared_ptr<Context> context) : context_(context) {
    if (!context_) {
        LOGE("Context pointer is null!");
        return;
    }
    createSwapchain();
    createSwapchainImages();
}

Swapchain::~Swapchain() {
    cleanup();
}

void Swapchain::cleanup() {
    // 通过context_访问device
    VkDevice device = context_->device_;
    if (device == VK_NULL_HANDLE) return;

    // 等待设备空闲
    vkDeviceWaitIdle(device);

    // 销毁信号量
    for (auto semaphore : imageAvailableSemaphores) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, semaphore, nullptr);
        }
    }
    imageAvailableSemaphores.clear();

    // 清理图像视图（Image本身由交换链管理）
    for (auto& img : swapchainImages) {
        if (img && img->imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, img->imageView, nullptr);
        }
    }
    swapchainImages.clear();

    // 销毁交换链
    if (vkSwapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, vkSwapchain, nullptr);
        vkSwapchain = VK_NULL_HANDLE;
    }
}

void Swapchain::createSwapchain() {
    // 通过context_访问所有需要的成员
    VkPhysicalDevice physicalDevice = context_->physicalDevice_;
    VkSurfaceKHR surface = context_->surface_;
    VkDevice device = context_->device_;
    
    if (physicalDevice == VK_NULL_HANDLE || surface == VK_NULL_HANDLE || device == VK_NULL_HANDLE) {
        LOGE("Invalid Vulkan handles in Context!");
        return;
    }

    // 查询交换链支持详情（通过context_的辅助函数）
    SwapChainSupportDetails supportDetails = context_->QuerySwapChainSupport(physicalDevice);
    
    // 选择最佳配置
    surfaceFormat = chooseSwapSurfaceFormat(supportDetails.formats);
    swapchainFormat = surfaceFormat.format;
    presentMode = chooseSwapPresentMode(supportDetails.presentModes);
    swapchainExtent = chooseSwapExtent(supportDetails.capabilities);

    // 决定图像数量
    imageCount = supportDetails.capabilities.minImageCount + 1;
    if (supportDetails.capabilities.maxImageCount > 0 && 
        imageCount > supportDetails.capabilities.maxImageCount) {
        imageCount = supportDetails.capabilities.maxImageCount;
    }

    LOGI("Creating swapchain: extent=%{public}dx%{public}d, format=%{public}d, "
         "presentMode=%{public}d, imageCount=%{public}d",
         swapchainExtent.width, swapchainExtent.height, 
         static_cast<int>(swapchainFormat), 
         static_cast<int>(presentMode), 
         imageCount);

    // 创建交换链
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = swapchainExtent;
    createInfo.imageArrayLayers = 1;
    // 关键：3DGS使用Compute Shader直接写入，必须包含STORAGE_BIT
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    createInfo.preTransform = supportDetails.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    // 判断队列族是否不同
    uint32_t graphicsFamily = static_cast<uint32_t>(context_->indices_.graphicsFamily);
    uint32_t presentFamily = static_cast<uint32_t>(context_->indices_.presentFamily);
    
    uint32_t queueFamilyIndices[] = {graphicsFamily, presentFamily};
    
    if (graphicsFamily != presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    VkResult result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &vkSwapchain);
    if (result != VK_SUCCESS) {
        LOGE("Failed to create swapchain: %{public}d", result);
        return;
    }

    LOGI("Swapchain created successfully");
}

void Swapchain::createSwapchainImages() {
    VkDevice device = context_->device_;
    if (device == VK_NULL_HANDLE) return;

    // 获取交换链图像
    uint32_t count = 0;
    vkGetSwapchainImagesKHR(device, vkSwapchain, &count, nullptr);
    std::vector<VkImage> images(count);
    vkGetSwapchainImagesKHR(device, vkSwapchain, &count, images.data());

    // 为每个图像创建视图
    swapchainImages.clear();
    for (size_t i = 0; i < images.size(); i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchainFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        VkResult result = vkCreateImageView(device, &viewInfo, nullptr, &imageView);
        if (result != VK_SUCCESS) {
            LOGE("Failed to create image view %{public}zu: %{public}d", i, result);
            continue;
        }

        swapchainImages.push_back(std::make_shared<Image>(
            images[i], imageView, swapchainFormat, swapchainExtent
        ));
    }

    // 创建图像可用信号量（每帧一个，用于vkAcquireNextImageKHR）
    imageAvailableSemaphores.resize(FRAMES_IN_FLIGHT);  // 使用FRAMES_IN_FLIGHT而不是imageCount
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        VkResult result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, 
                                           &imageAvailableSemaphores[i]);
        if (result != VK_SUCCESS) {
            LOGE("Failed to create semaphore %{public}zu: %{public}d", i, result);
        }
    }

    LOGI("Created %{public}zu swapchain images with views and %{public}d semaphores", 
         swapchainImages.size(), FRAMES_IN_FLIGHT);
}

void Swapchain::recreate() {
    LOGI("Recreating swapchain...");
    
    // 清理旧资源
    cleanup();
    
    // 重新创建
    createSwapchain();
    createSwapchainImages();
}

VkSurfaceFormatKHR Swapchain::chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    
    // 优先选择B8G8R8A8_UNORM + SRGB非线性
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR Swapchain::chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& availablePresentModes) {
    
    // 如果配置要求immediate且可用，使用它
    if (context_->config_.immediateSwapchain) {
        for (const auto& mode : availablePresentModes) {
            if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                return mode;
            }
        }
    }
    
    // 优先Mailbox（低延迟），否则FIFO（VSync）
    for (const auto& mode : availablePresentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }
    
    return VK_PRESENT_MODE_FIFO_KHR;  // 必支持
}

VkExtent2D Swapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    // 如果当前范围有效，直接使用
    if (capabilities.currentExtent.width != UINT32_MAX && 
        capabilities.currentExtent.height != 0) {
        return capabilities.currentExtent;
    }
    
    // 从OHNativeWindow获取实际大小
    // 注意：这里可以使用OH_NativeWindow_GetSurfaceId等API获取具体尺寸
    // 简化处理：使用minImageExtent和maxImageExtent之间的默认值
    VkExtent2D actualExtent = capabilities.currentExtent;
    
    if (actualExtent.width == 0 || actualExtent.height == 0) {
        actualExtent.width = context_->config_.image_width;
        actualExtent.height = context_->config_.image_height;
        
        // 限制在范围内
        actualExtent.width = std::clamp(actualExtent.width,
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height,
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height);
    }
    
    return actualExtent;
}
#ifndef VULKAN_SPLATTING_SWAPCHAIN_H
#define VULKAN_SPLATTING_SWAPCHAIN_H

#include "Context.h"

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

// Image结构体（与原实现保持一致）
struct Image {
    VkImage image;
    VkImageView imageView;
    VkFormat format;
    VkExtent2D extent;
    
    Image(VkImage img, VkImageView view, VkFormat fmt, VkExtent2D ext)
        : image(img), imageView(view), format(fmt), extent(ext) {}
};

class Swapchain {
public:
    explicit Swapchain(std::shared_ptr<Context> context);
    
    ~Swapchain();

    // 交换链重建
    void recreate();
    
    // 获取信息
    uint32_t getImageCount() const { return static_cast<uint32_t>(swapchainImages.size()); }
    VkExtent2D getExtent() const { return swapchainExtent; }
    VkFormat getFormat() const { return swapchainFormat; }
    
    // 获取指定帧的图像可用信号量
    VkSemaphore getImageAvailableSemaphore(uint32_t frameIndex) const {
        if (frameIndex < imageAvailableSemaphores.size()) {
            return imageAvailableSemaphores[frameIndex];
        }
        return VK_NULL_HANDLE;
    }

    VkSwapchainKHR vkSwapchain = VK_NULL_HANDLE;
    VkExtent2D swapchainExtent = {};
    std::vector<std::shared_ptr<Image>> swapchainImages;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    VkSurfaceFormatKHR surfaceFormat = {};
    VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    uint32_t imageCount = 0;
    
private:
    std::shared_ptr<Context> context_;  // 指向Context的指针，用于访问其public成员
    
    // 内部创建函数
    void createSwapchain();
    void createSwapchainImages();
    void cleanup();
    
    // 辅助选择函数
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
};

#endif // VULKAN_SPLATTING_SWAPCHAIN_H
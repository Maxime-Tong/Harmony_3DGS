#ifndef HARMONYOS_3DGS_VULKAN3DGS_H
#define HARMONYOS_3DGS_VULKAN3DGS_H

#include "Context.h"
#include "DescriptorSet.h"
#include "GSScene.h"
#include "Swapchain.h"
#include "XEngineSorter.h"
#include "pipelines/ComputePipeline.h"

#include "glm/gtc/quaternion.hpp"

#include <cstdint>
#include <memory>

struct alignas(16) UniformBuffer {
    glm::vec4 camera_position;
    glm::mat4 proj_mat;
    glm::mat4 view_mat;
    uint32_t width;
    uint32_t height;
    float tan_fovx;
    float tan_fovy;
};

struct VertexAttributeBuffer {
    glm::vec4 conic_opacity;
    glm::vec4 color_radii;
    glm::uvec4 aabb;
    glm::vec2 uv;
    float depth;
    uint32_t __padding[1];
};

struct TileDepth {
    uint32_t low;
    uint32_t high;
};

struct Camera {
    glm::vec3 position;
    glm::quat rotation;
    float fov;
    float nearPlane;
    float farPlane;

    void translate(glm::vec3 translation) {
        position += rotation * translation;
    }
};

struct RadixSortPushConstants {
    uint32_t g_num_elements; // == NUM_ELEMENTS
    uint32_t g_shift; // (*)
    uint32_t g_num_workgroups; // == NUMBER_OF_WORKGROUPS as defined in the section above
    uint32_t g_num_blocks_per_workgroup; // == NUM_BLOCKS_PER_WORKGROUP
};

class Vulkan3DGS {
public:
    Vulkan3DGS() = default;
    
    void setConfig(uint64_t width, uint64_t height, OHNativeWindow* window, NativeResourceManager* resourceManager);
    void initialize();
    void initializeVulkan();
    void loadScene();
    void createPreprocessPipeline();
    void createPrefixSumPipeline();
    void createRadixSortPipeline();
    void createXEngineSorter();
    void createPreprocessSortPipeline();
    void createTileBoundaryPipeline();
    void createRenderPipeline();
    void createCommandPool();
    void recordPreprocessCommandBuffer();
    bool recordRenderCommandBuffer(uint32_t currentFrame);
    
    void loadTestCameras();
    void recreateSwapchain();
    void updateUniforms();
    void draw();
    void waitDeviceIde();

private:
    bool initialized_ = false;

    RendererConfiguration config_;
    std::shared_ptr<Context> context;
    std::shared_ptr<GSScene> scene;
    std::shared_ptr<Swapchain> swapchain;
    std::shared_ptr<XEngineSorter> sorter;

    VkCommandPool commandPool_;
    VkCommandBuffer preprocessCommandBuffer_;
    VkCommandBuffer renderCommandBuffer_;
    
    std::shared_ptr<ComputePipeline> preprocessPipeline;
    std::shared_ptr<ComputePipeline> renderPipeline;
    std::shared_ptr<ComputePipeline> prefixSumPipeline;
    std::shared_ptr<ComputePipeline> sortHistPipeline;
    std::shared_ptr<ComputePipeline> sortPipeline;
    std::shared_ptr<ComputePipeline> preprocessSortPipeline;
    std::shared_ptr<ComputePipeline> tileBoundaryPipeline;

    std::shared_ptr<Buffer> uniformBuffer;
    std::shared_ptr<Buffer> vertexAttributeBuffer;
    // std::shared_ptr<Buffer> tileOverlapBuffer;
    std::shared_ptr<Buffer> prefixSumPingBuffer;
    std::shared_ptr<Buffer> prefixSumPongBuffer;
    std::shared_ptr<Buffer> sortCountBuffer;
    std::shared_ptr<Buffer> sortKeyBufferEven;
    std::shared_ptr<Buffer> sortKeyBufferOdd;
    std::shared_ptr<Buffer> sortHistBuffer;
    std::shared_ptr<Buffer> sortValueBufferEven;
    std::shared_ptr<Buffer> sortValueBufferOdd;
    std::shared_ptr<Buffer> totalSumBufferHost;
    std::shared_ptr<Buffer> tileBoundaryBuffer;

    std::shared_ptr<DescriptorSet> inputSet;
    
    uint32_t numRadixSortBlocksPerWorkgroup = 32;
    unsigned int sortBufferSizeMultiplier = 8;
    
    uint32_t currentImageIndex;
    
    std::vector<Camera> testCameras;
    uint32_t testCameraIndex = 0;
    uint32_t direction = 1;
    
    Camera camera {
        .position = glm::vec3(0.0f, 0.0f, 0.0f),
        // .position =  glm::vec3(2.740232f, -1.044583f, 2.176332f),
        .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        // .rotation = glm::quat(-0.563956f, -0.041806f, 0.722993f, 0.392203f),
        .fov = 45.0f,
        .nearPlane = 0.1f,
        .farPlane = 1000.0f,
    };
};

#endif
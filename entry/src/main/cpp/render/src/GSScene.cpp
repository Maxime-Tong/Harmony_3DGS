//
// Created by steven on 11/30/23.
//
#include "GSScene.h"
#include "Shader.h"
#include "pipelines/ComputePipeline.h"
#include "shaders_half.h"
#include "Utils.h"

#include <sstream>
#include <string_view>

#include <random>

#include <hilog/log.h>

#undef LOG_TAG
#define LOG_TAG "GSScene"

#undef LOG_DOMAIN
#define LOG_DOMAIN 0xFF03

#define LOGI(...) OH_LOG_INFO(LOG_APP, __VA_ARGS__)
#define LOGE(...) OH_LOG_ERROR(LOG_APP, __VA_ARGS__)


struct VertexStorage {
    glm::vec3 position;
    glm::vec3 normal;
    float shs[48];
    float opacity;
    glm::vec3 scale;
    glm::vec4 rotation;
};

void GSScene::load(const std::shared_ptr<Context>&context) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    std::vector<char> fileBuffer = VulkanUtils::readFile(context->config_.resourceManager_, filename);
    if (fileBuffer.empty()) {
        LOGE("Failed to load PLY: Buffer is empty.");
        return;
    }
    size_t dataOffset = loadPlyHeader(fileBuffer);
    
    vertexBuffer = createBuffer(context, header.numVertices * sizeof(Vertex));
    auto vertexStagingBuffer = Buffer::staging(context, header.numVertices * sizeof(Vertex));
    auto* targetVertices = static_cast<Vertex *>(vertexStagingBuffer->allocation_info.pMappedData);
    
    const char* binaryDataPtr = fileBuffer.data() + dataOffset;
    for (int i = 0; i < header.numVertices; i++) {
        const auto* src = reinterpret_cast<const VertexStorage*>(binaryDataPtr + i * sizeof(VertexStorage));

        targetVertices[i].position = glm::vec4(src->position, 1.0f);

        glm::vec3 scale = glm::exp(src->scale);
        float opacity = 1.0f / (1.0f + std::exp(-src->opacity));
        targetVertices[i].scale_opacity = glm::vec4(scale, opacity);

        targetVertices[i].rotation = glm::normalize(src->rotation);

        targetVertices[i].shs[0] = src->shs[0]; // R0
        targetVertices[i].shs[1] = src->shs[1]; // G0
        targetVertices[i].shs[2] = src->shs[2]; // B0
        
        const int SH_N = 16;
        for (int j = 1; j < SH_N; j++) {
            targetVertices[i].shs[j * 3 + 0] = src->shs[(j - 1) + 3];        // R_j
            targetVertices[i].shs[j * 3 + 1] = src->shs[(j - 1) + SH_N + 2]; // G_j
            targetVertices[i].shs[j * 3 + 2] = src->shs[(j - 1) + SH_N * 2 + 1]; // B_j
        }
    }

    vertexBuffer->uploadFrom(vertexStagingBuffer);

    auto endTime = std::chrono::high_resolution_clock::now();
    LOGI("Loaded %{public}s in %{public}lldms", filename.c_str(),
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count());
    LOGI("Vertex count: %{public}d", header.numVertices);
    
    precomputeCov3D(context);
}

void GSScene::loadTestScene(const std::shared_ptr<Context>&context) {
    int testObects = 1;
    header.numVertices = testObects;
    vertexBuffer = createBuffer(context, testObects * sizeof(Vertex));
    auto vertexStagingBuffer = Buffer::staging(context, testObects * sizeof(Vertex));
    auto* verteces = static_cast<Vertex *>(vertexStagingBuffer->allocation_info.pMappedData);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> posgen(-3.0, 3.0);
    std::uniform_real_distribution<> scalegen(100.0, 5000.0);
    std::uniform_real_distribution<> shsgen(-1.0, 1.0);


    for (auto i = 0; i < testObects; i++) {
        verteces[i].position = glm::vec4(posgen(gen), posgen(gen), posgen(gen), 1.0f);
        // verteces[i].normal = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        verteces[i].scale_opacity = glm::vec4(scalegen(gen), scalegen(gen), scalegen(gen), 0.5f);
        verteces[i].rotation = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        for (auto j = 0; j < 48; j++) {
            verteces[i].shs[j] = shsgen(gen);
        }
    }

    vertexBuffer->uploadFrom(vertexStagingBuffer);

    precomputeCov3D(context);
}

size_t GSScene::loadPlyHeader(const std::vector<char>& buffer) {
    if (buffer.empty()) {
        throw std::runtime_error("Buffer is empty");
    }

    // 将整个 buffer 视为字符串流解析（只处理头部文本部分）
    std::string bufferStr(buffer.data(), buffer.size());
    std::stringstream ss(bufferStr);
    
    std::string line;
    bool headerEnd = false;
    size_t headerTotalBytes = 0;
    
    // 记录当前正在解析哪个元素的属性 (0: none, 1: vertex, 2: face)
    int currentElement = 0; 

    while (std::getline(ss, line)) {
        // 更新已读取的字节数 (注意：getline 丢弃了换行符，需补回长度)
        headerTotalBytes += line.length() + 1; 

        // 去掉行尾回车符（兼容 Windows 格式）
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if (token == "ply") {
            continue;
        }
        else if (token == "format") {
            iss >> header.format;
        }
        else if (token == "element") {
            std::string elementName;
            iss >> elementName;
            if (elementName == "vertex") {
                iss >> header.numVertices;
                currentElement = 1;
            }
            else if (elementName == "face") {
                iss >> header.numFaces;
                currentElement = 2;
            }
        }
        else if (token == "property") {
            PlyProperty property;
            // 处理常见的 list 类型，例如 property list uchar int vertex_indices
            std::string typeOrList;
            iss >> typeOrList;
            
            if (typeOrList == "list") {
                std::string countType, itemType;
                iss >> countType >> itemType >> property.name;
                property.type = "list"; // 标记为列表
            } else {
                property.type = typeOrList;
                iss >> property.name;
            }

            // 根据当前元素将属性归类
            if (currentElement == 1) {
                header.vertexProperties.push_back(property);
            }
            else if (currentElement == 2) {
                header.faceProperties.push_back(property);
            }
        }
        else if (token == "end_header") {
            headerEnd = true;
            break;
        }
    }

    if (!headerEnd) {
        LOGE("Could not find end_header in PLY file");
        throw std::runtime_error("Invalid PLY header");
    }

    return headerTotalBytes;
}

std::shared_ptr<Buffer> GSScene::createBuffer(const std::shared_ptr<Context>&context, size_t i) {
    return std::make_shared<Buffer>(
        context, i, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, false);
}

void GSScene::precomputeCov3D(const std::shared_ptr<Context>&context) {
    cov3DBuffer = createBuffer(context, header.numVertices * sizeof(float) * 6);

    auto pipeline = std::make_shared<ComputePipeline>(
        context, std::make_shared<Shader>(context, "precomp_cov3d", SPV_PRECOMP_COV3D, SPV_PRECOMP_COV3D_len));

    auto descriptorSet = std::make_shared<DescriptorSet>(context, FRAMES_IN_FLIGHT);
    descriptorSet->bindBufferToDescriptorSet(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT,
                                             vertexBuffer);
    descriptorSet->bindBufferToDescriptorSet(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT,
                                             cov3DBuffer);
    descriptorSet->build();

    pipeline->addDescriptorSet(0, descriptorSet);
    pipeline->addPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float));
    pipeline->build();

    auto commandBuffer = context->beginOneTimeCommandBuffer();
    pipeline->bind(commandBuffer, 0, 0);
    
    float scaleFactor = 1.0f;
    vkCmdPushConstants(commandBuffer, pipeline->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float), &scaleFactor);

    uint32_t numGroups = (header.numVertices + 255) / 256;
    vkCmdDispatch(commandBuffer, numGroups, 1, 1);

    // 提交命令
    context->endOneTimeCommandBuffer(commandBuffer, context->computeQueue_);

    LOGI("Precomputed Cov3D");
}

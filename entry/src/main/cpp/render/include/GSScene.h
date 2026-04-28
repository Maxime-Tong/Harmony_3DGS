//
// Created on 2026/4/7.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef HARMONYOS_3DGS_GSSCENE_H
#define HARMONYOS_3DGS_GSSCENE_H

#include "Context.h"
#include "Buffer.h"

#include <glm/glm.hpp>

struct PlyProperty {
    std::string type;
    std::string name;
};

struct PlyHeader {
    std::string format;
    int numVertices;
    int numFaces;
    std::vector<PlyProperty> vertexProperties;
    std::vector<PlyProperty> faceProperties;
};

class GSScene {
public:
    explicit GSScene(const std::string& filename)
        : filename(filename) {}

    void load(const std::shared_ptr<Context>& context);

    void loadTestScene(const std::shared_ptr<Context>& context);

    uint64_t getNumVertices() const {
        return header.numVertices;
    }

    struct Vertex {
        glm::vec4 position;
        glm::vec4 scale_opacity;
        glm::vec4 rotation;
        float shs[48];
    };

    struct Cov3DUpperRight {
        float mat[6];
    };

    std::shared_ptr<Buffer> vertexBuffer;
    std::shared_ptr<Buffer> cov3DBuffer;
private:
    std::string filename;
    PlyHeader header;

//    std::shared_ptr<Buffer> createStagingBuffer(const std::shared_ptr<Context>& sharedPtr, unsigned long i);

    size_t loadPlyHeader(const std::vector<char>& buffer);

    static std::shared_ptr<Buffer> createBuffer(const std::shared_ptr<Context>& sharedPtr, size_t i);

    void precomputeCov3D(const std::shared_ptr<Context>& context);
};

#endif //HARMONYOS_3DGS_GSSCENE_H

#ifndef HARMONYOS_3DGS_SHADER_H
#define HARMONYOS_3DGS_SHADER_H

#include <memory>
#include <string>
#include <utility>
#include <vulkan/vulkan.h>
#include "Context.h"

class Shader {
public:
    Shader(const std::shared_ptr<Context>& _context, std::string filename)
        : context(_context),
          filename(std::move(filename)) {
    }

    Shader(const std::shared_ptr<Context>& _context, const unsigned char * data, size_t size)
        : context(_context),
          filename(""),
          data(data),
          size(size) {
    }

    Shader(const std::shared_ptr<Context>& context, const std::string& filename, const unsigned char * data, size_t size)
        : filename(filename),
          context(context),
          data(data),
          size(size) {
    }

    // 需要添加析构函数释放原生句柄
    ~Shader();

    void load();

    // 改为原生句柄
    VkShaderModule vkShaderModule = VK_NULL_HANDLE;

private:
    const std::string filename;
    std::shared_ptr<Context> context; // 确保类名一致
    const unsigned char* data = nullptr;
    size_t size = 0;
};

#endif //HARMONYOS_3DGS_SHADER_H
//
// Created on 2026/4/26.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef HARMONYOS_3DGS_XENGINESORTER_H
#define HARMONYOS_3DGS_XENGINESORTER_H
#include <algorithm>
#include <vector>
#include <string>
#include <xengine/xeg_vulkan_hps.h>
#include <xengine/xeg_vulkan_extension.h>
#include <xengine/xeg_extension_defs.h>

class Context;
class Buffer;

class XEngineSorter {
public:
    XEngineSorter(const std::shared_ptr<Context>& _context);
    ~XEngineSorter();

    void cmdDispatchSort(VkCommandBuffer cmdBuffer,
                        std::shared_ptr<Buffer> keyBuffer, 
                        std::shared_ptr<Buffer> valueBuffer,
                        std::shared_ptr<Buffer> sortCount);

private:
    std::shared_ptr<Context> context;
    XEG_HPS sorter_ = VK_NULL_HANDLE;
};

#endif //HARMONYOS_3DGS_XENGINESORTER_H

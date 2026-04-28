//
// Created on 2026/4/15.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "Utils.h"

#include <string>
#include <hilog/log.h>

#undef LOG_TAG
#define LOG_TAG "Utils"

#undef LOG_DOMAIN
#define LOG_DOMAIN 0xF000

#define LOGI(...) OH_LOG_INFO(LOG_APP, __VA_ARGS__)
#define LOGE(...) OH_LOG_ERROR(LOG_APP, __VA_ARGS__)

namespace VulkanUtils {
std::vector<char> readFile(NativeResourceManager* resMgr, const std::string& filename) {
    if (resMgr == nullptr) {
        LOGE("Resource Manager is null");
        return {};
    }

    RawFile* rawFile = OH_ResourceManager_OpenRawFile(resMgr, filename.c_str());
    if (rawFile == nullptr) {
        LOGE("Failed to open rawfile: %s", filename.c_str());
        return {};
    }

    long len = OH_ResourceManager_GetRawFileSize(rawFile);
    std::vector<char> buffer(len);

    int readLen = OH_ResourceManager_ReadRawFile(rawFile, buffer.data(), len);
    
    OH_ResourceManager_CloseRawFile(rawFile);

    if (readLen != len) {
        return {};
    }

    return buffer;
}

}

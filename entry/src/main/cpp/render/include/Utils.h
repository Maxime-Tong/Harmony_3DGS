//
// Created on 2026/4/15.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef HARMONYOS_3DGS_UTILS_H
#define HARMONYOS_3DGS_UTILS_H

#include <rawfile/raw_file_manager.h>
#include <vector>

namespace VulkanUtils {
std::vector<char> readFile(NativeResourceManager* resMgr, const std::string& filename);
};

#endif //HARMONYOS_3DGS_UTILS_H

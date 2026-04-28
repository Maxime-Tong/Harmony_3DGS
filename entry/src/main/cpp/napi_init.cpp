/*
 * 基于 NAPI 的鸿蒙 Vulkan 3DGS 模块
 */
#include <node_api.h>
#include "PluginRender.h"

EXTERN_C_START

static napi_value Init(napi_env env, napi_value exports) {
    PluginRender::GetInstance().NapiInit(env, exports);
    
    napi_property_descriptor desc[] = {
        {"initContext", nullptr, PluginRender::initContext, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"start", nullptr, PluginRender::StartRender, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"stop", nullptr, PluginRender::StopRender, nullptr, nullptr, nullptr, napi_default, nullptr}
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);

    return exports;
}

EXTERN_C_END

static napi_module vulkan3dgs = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "vulkan3dgs",  // <--- TS 中 import 的名字
    .nm_priv = nullptr,
    .reserved = {0},
};

// 自动注册模块
extern "C" __attribute__((constructor)) void RegisterVulkan3DGSModule()
{
    napi_module_register(&vulkan3dgs);
}
//
// Created on 2026/4/22.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".
#include <cstdio>
#include <hilog/log.h>

#include "PluginRender.h"
#include "Vulkan3DGS.h"

#undef LOG_TAG
#define LOG_TAG "PluginRender"

#undef LOG_DOMAIN
#define LOG_DOMAIN 0xFFF0

#define LOGI(...) OH_LOG_INFO(LOG_APP, __VA_ARGS__)
#define LOGE(...) OH_LOG_ERROR(LOG_APP, __VA_ARGS__)

PluginRender PluginRender::instance_;

PluginRender& PluginRender::GetInstance() {
    return instance_;
}

PluginRender::PluginRender()
    : isStop_(true)
    , vulkanRender_(std::make_unique<Vulkan3DGS>()) {}

PluginRender::~PluginRender() {
    Stop();
}

void PluginRender::NapiInit(napi_env env, napi_value exports) {
    if (!env || !exports) return;
    
    // Set xcomponent and register callbacks
    napi_value xcomp = nullptr;
    if (napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &xcomp) != napi_ok) return;
    
    if (napi_unwrap(env, xcomp, reinterpret_cast<void**>(&nativeXComponent_)) != napi_ok) return;
    
    renderCallback_.OnSurfaceCreated = OnSurfaceCreatedCB;
    renderCallback_.OnSurfaceChanged = OnSurfaceChangedCB;
    renderCallback_.OnSurfaceDestroyed = OnSurfaceDestroyedCB;

    OH_NativeXComponent_RegisterCallback(nativeXComponent_, &renderCallback_);
}

napi_value PluginRender::initContext(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) return nullptr;

    GetInstance().resourceManager_ = OH_ResourceManager_InitNativeResourceManager(env, args[0]);
    
    if (GetInstance().resourceManager_ != nullptr) {
        LOGI("ResourceManager initialized successfully from ArkTS");
    }
    
    return nullptr;
}

napi_value PluginRender::StartRender(napi_env env, napi_callback_info info) {
    PluginRender::GetInstance().Start();
    
    napi_value result;
    napi_get_undefined(env, &result);
    return result;
}

napi_value PluginRender::StopRender(napi_env env, napi_callback_info info) {
    PluginRender::GetInstance().Stop();
    
    napi_value result;
    napi_get_undefined(env, &result);
    return result;
}

void PluginRender::OnSurfaceCreatedCB(OH_NativeXComponent* component, void* window) {
    PluginRender& render = PluginRender::GetInstance();
    render.nativeWindow_ = static_cast<OHNativeWindow*>(window);

    uint64_t targetWidth = 720;
    uint64_t targetHeight = 1280;

    int32_t ret = OH_NativeWindow_NativeWindowHandleOpt(
        render.nativeWindow_, 
        SET_BUFFER_GEOMETRY, 
        targetWidth, 
        targetHeight
    );

    if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        LOGE("设置 Buffer 几何尺寸失败");
    }

    // 3. 将 720p 尺寸传递给 Vulkan 渲染器
    // 这样 Vulkan 的 Swapchain 和 RenderPass 都会基于 720p 创建
    LOGI("PluginRender: 渲染分辨率 (%{public}d x %{public}d)", targetWidth, targetHeight);
    render.vulkanRender_->setConfig(targetWidth, targetHeight, render.nativeWindow_, render.resourceManager_);
    render.vulkanRender_->initialize();
}

void PluginRender::OnSurfaceChangedCB(OH_NativeXComponent* component, void* window) {
    LOGI("PluginRender: 窗口尺寸变化\n");
}

void PluginRender::OnSurfaceDestroyedCB(OH_NativeXComponent* component, void* window) {
    PluginRender::GetInstance().Stop();
    LOGI("PluginRender: 窗口销毁，渲染线程已停止\n");
}

void PluginRender::Start() {
    std::lock_guard<std::mutex> lock(renderMutex_);
    if (!isStop_) return;

    isStop_ = false;
    renderThread_ = std::thread(&PluginRender::RenderLoop, this);
    LOGI("PluginRender: 渲染线程已启动\n");
}

void PluginRender::Stop() {
    std::lock_guard<std::mutex> lock(renderMutex_);
    if (isStop_) return;

    isStop_ = true;
    if (renderThread_.joinable()) {
        renderThread_.join();
    }
    LOGI("PluginRender: 渲染线程已停止\n");
}

// 渲染循环
void PluginRender::RenderLoop() {
    while (!isStop_) {
        // 调用Vulkan渲染帧
        vulkanRender_->draw();
    }
    vulkanRender_->waitDeviceIde();
}

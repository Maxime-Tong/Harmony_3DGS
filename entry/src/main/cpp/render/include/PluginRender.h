//
// Created on 2026/4/22.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#ifndef HARMONYOS_3DGS_PLUGINRENDER_H
#define HARMONYOS_3DGS_PLUGINRENDER_H

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <node_api.h>
#include <native_window/external_window.h>
#include <atomic>
#include <mutex>
#include <rawfile/raw_file_manager.h>
#include <thread>
#include <memory>

class Vulkan3DGS;

class PluginRender {
public:
    static PluginRender& GetInstance();
    PluginRender(const PluginRender&) = delete;
    PluginRender& operator=(const PluginRender&) = delete;

    void NapiInit(napi_env env, napi_value exports);
    
    static napi_value initContext(napi_env env, napi_callback_info info);
    static napi_value StartRender(napi_env env, napi_callback_info info);
    static napi_value StopRender(napi_env env, napi_callback_info info);

    void Start();
    void Stop();

    OH_NativeXComponent* GetNativeXComponent() const { return nativeXComponent_; }
    OHNativeWindow* GetNativeWindow() const { return nativeWindow_; }
    NativeResourceManager* GetResourceManager() const { return resourceManager_; }

private:
    PluginRender();
    ~PluginRender();

    void RenderLoop();
    static void OnSurfaceCreatedCB(OH_NativeXComponent* component, void* window);
    static void OnSurfaceChangedCB(OH_NativeXComponent* component, void* window);
    static void OnSurfaceDestroyedCB(OH_NativeXComponent* component, void* window);

private:
    // 单例实例
    static PluginRender instance_;
    
    OH_NativeXComponent_Callback renderCallback_;
    OH_NativeXComponent* nativeXComponent_ = nullptr;
    OHNativeWindow* nativeWindow_ = nullptr;
    NativeResourceManager* resourceManager_ = nullptr;

    std::thread renderThread_;
    std::atomic<bool> isStop_;
    std::mutex renderMutex_;

    std::unique_ptr<Vulkan3DGS> vulkanRender_;
};

#endif //HARMONYOS_3DGS_PLUGINRENDER_H

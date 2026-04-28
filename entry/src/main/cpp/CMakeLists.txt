# the minimum version of CMake.
cmake_minimum_required(VERSION 3.5.0)
set(CMAKE_CXX_STANDARD 17)
project(HarmonyOS_3DGS)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
#add_definitions(-DOHOS_PLATFORM)

ADD_DEFINITIONS(-DVK_USE_PLATFORM_OHOS=1)
#ADD_DEFINITIONS(-DVK_NO_PROTOTYPES=1)

include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/render/include
                    ${NATIVERENDER_ROOT_PATH}/shaders
                    ${NATIVERENDER_ROOT_PATH}/third_party)

set(BASE_LIBRARY
    # ACE能力
    libace_napi.z.so        # NAPI接口，用于ArkTS/JS交互
    libace_ndk.z.so         # ACE NDK能力
    librawfile.z.so

    libvulkan.so

    # 基础工具
    libuv.so                # 异步IO/事件循环
    libhilog_ndk.z.so       # 日志输出
    
    # Vulkan/图形相关
    libnative_window.so     # 窗口表面管理
    libnative_image.so      # 图像管理
    libnative_buffer.so     # 缓冲区管理
    libnative_vsync.so      # 垂直同步
)

find_library(
    # Sets the name of the path variable.
    xengine-lib
    # Specifies the name of the NDK library that you want CMake to locate.
    xengine
)

add_library(vulkan3dgs SHARED
            napi_init.cpp
            render/src/Vulkan3DGS.cpp
            render/src/Context.cpp
            render/src/Swapchain.cpp
            render/src/GSScene.cpp
            render/src/Buffer.cpp
            render/src/DescriptorSet.cpp
            render/src/Shader.cpp
            third_party/json/jsoncpp.cpp
            render/include/pipelines/Pipeline.cpp
            render/include/pipelines/ComputePipeline.cpp
            render/src/Utils.cpp
            render/src/PluginRender.cpp
            render/src/XEngineSorter.cpp)

target_link_libraries(vulkan3dgs PUBLIC ${BASE_LIBRARY} ${xengine-lib})
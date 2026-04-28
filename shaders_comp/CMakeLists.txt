cmake_minimum_required(VERSION 3.26)
project(shaders)

find_program(Vulkan_GLSLANG_VALIDATOR_EXECUTABLE
    NAMES glslangValidator glslangValidator.exe
)

# 1. 编译嵌入程序工具
# 确保该工具能处理文件的追加(Append)写入
add_executable(embedfile ${CMAKE_SOURCE_DIR}/cmake/embedfile.c)

# 2. 自动搜索当前目录及子目录下的所有 Compute Shader
file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.comp"
)

# 3. 配置编译宏 (仅保留 Debug/Release 判断)
set(GLSLC_DEFINE "")
if (CMAKE_BUILD_TYPE MATCHES Debug)
    list(APPEND GLSLC_DEFINE "-DDEBUG")
else ()
    list(APPEND GLSLC_DEFINE "-DNDEBUG")
endif ()

# 4. 定义路径
set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
set(SHADER_HEADER "${SHADER_OUTPUT_DIR}/shaders.h")

# 确保输出目录存在
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

# 5. 遍历并为每个 Shader 生成编译指令
set(SPIRV_BINARY_FILES "")
set(SHADER_VARS "")
set(SHADER_SPVS "")

add_custom_command(
    OUTPUT ${SHADER_HEADER}
    COMMAND ${CMAKE_COMMAND} -E echo "#pragma once" > ${SHADER_HEADER}
    DEPENDS ${SPIRV_BINARY_FILES}
    COMMENT "Initializing ${SHADER_HEADER}"
)

foreach (GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME_WE)
    get_filename_component(EXT ${GLSL} LAST_EXT)
    
    # 变量名规范化: 如 preprocess.comp -> SPV_PREPROCESS
    string(TOUPPER ${FILE_NAME} FILE_NAME_UPPER)
    set(VAR_NAME "SPV_${FILE_NAME_UPPER}")
    
    set(SPIRV "${SHADER_OUTPUT_DIR}/${FILE_NAME}.spv")
    
    # 核心指令：GLSL -> SPIR-V
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
        COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} "--target-env" "vulkan1.2" -V ${GLSL} -o ${SPIRV} ${GLSLC_DEFINE}
        DEPENDS ${GLSL}
        COMMENT "Compiling ${GLSL}"
    )
    
    add_custom_command(
        OUTPUT ${SHADER_HEADER}
        COMMAND embedfile ${VAR_NAME} ${SPIRV} ${SHADER_HEADER}
        DEPENDS ${SPIRV}
        APPEND
    )

    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach ()

# 7. 定义顶层 Target
add_custom_target(shaders ALL DEPENDS ${SHADER_HEADER} ${SPIRV_BINARY_FILES})
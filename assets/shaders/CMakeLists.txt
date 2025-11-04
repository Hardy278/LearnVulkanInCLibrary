cmake_minimum_required(VERSION 4.0.0)

find_package(Vulkan REQUIRED)

# ------------------ 设置源文件和目标文件路径 ------------------
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(STAGE_VERT -fshader-stage=vert)
set(STAGE_FRAG -fshader-stage=frag)

# ------------------------ 生成SPV文件 -------------------------
# 收集所有的.glsl文件
file(GLOB GLSL_FILES "*.glsl")
set(SPV_FILES "")
# 为每个GLSL文件创建编译规则
foreach(GLSL_FILE IN LISTS GLSL_FILES)
    # 根据文件扩展名确定着色器阶段并生成SPV文件名
    get_filename_component(FILE_EXT ${GLSL_FILE} EXT)
    if(FILE_EXT STREQUAL ".vert.glsl")
        get_filename_component(FILE_NAME ${GLSL_FILE} NAME_WE)
        set(SPV_FILE "${SHADER_DIR}/${FILE_NAME}.vert.spv")
        list(APPEND SPV_FILES ${SPV_FILE})
        set(SHADER_STAGE ${STAGE_VERT})
    elseif(FILE_EXT STREQUAL ".frag.glsl")
        get_filename_component(FILE_NAME ${GLSL_FILE} NAME_WE)
        set(SPV_FILE "${SHADER_DIR}/${FILE_NAME}.frag.spv")
        list(APPEND SPV_FILES ${SPV_FILE})
        set(SHADER_STAGE ${STAGE_FRAG})
    endif()
    # 创建编译命令
    add_custom_command(
        OUTPUT ${SPV_FILE}
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${SHADER_STAGE} ${GLSL_FILE} -o ${SPV_FILE}
        COMMENT "Compiling ${GLSL_FILE} to ${SPV_FILE}"
        DEPENDS ${GLSL_FILE}
    )
endforeach()

# ------------------------ 复制SPV文件 -------------------------
add_custom_target(CompileShaders ALL DEPENDS ${SPV_FILES})
# 创建输出目录
set(OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/shaders")
file(MAKE_DIRECTORY ${OUTPUT_DIR})
# 复制所有生成的SPV文件到输出目录
foreach(SPV_FILE IN LISTS SPV_FILES)
    get_filename_component(FILE_NAME ${SPV_FILE} NAME)
    add_custom_command(
        TARGET CompileShaders POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SPV_FILE} ${OUTPUT_DIR}/${FILE_NAME}
    )
endforeach()
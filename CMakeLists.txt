cmake_minimum_required(VERSION 4.0.0)

set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")

project(LearnVulkan VERSION 0.1.0 LANGUAGES C CXX)
# 设置编译器警告级别
if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Werror)
endif()

# 设置编译输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(TARGET ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME})

# 设置C++标准
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 搜索并链接库
find_package(SDL3 REQUIRED)          # SDL库
find_package(SDL3_image REQUIRED)    # SDL图像库
find_package(SDL3_ttf REQUIRED)      # SDL字体库
find_package(glm REQUIRED)           # 数学库
find_package(Vulkan REQUIRED)        # Vulkan库
find_package(spdlog REQUIRED)        # 日志库

# 添加可执行程序目标
set(SOURCES
    src/main.cpp

    src/engine/core/GameApp.cpp
    src/engine/core/Time.cpp

    src/engine/render/VulkanRenderer.cpp
)
add_executable(${TARGET} ${SOURCES})

# 链接库
target_link_libraries(${TARGET} PRIVATE
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
    Vulkan::Vulkan
    glm::glm
    spdlog::spdlog
)

# 添加子目录
add_subdirectory(assets)
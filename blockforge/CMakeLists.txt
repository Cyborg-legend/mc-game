cmake_minimum_required(VERSION 3.18)
project(BlockForge CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# C++17 features
set(CMAKE_CXX_EXTENSIONS OFF)

# Choose renderer: Vulkan or OpenGL
option(BLOCKFORGE_VULKAN_RENDERER "Use Vulkan renderer (default: OFF for OpenGL)" OFF)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

# External dependencies (header-only)
# nlohmann/json, stb_image, miniaudio are header-only

add_subdirectory(core)
add_subdirectory(renderer)
add_subdirectory(platform)
add_subdirectory(audio)
add_subdirectory(assets)

add_executable(blockforge main.cpp)
target_link_libraries(blockforge PRIVATE
    core
    renderer
    platform
)

# Include vulkan and glfw headers globally
target_include_groups(blockforge PUBLIC
    $<BUILD_INTERFACE:${Vulkan_INCLUDE_DIRS}>
    $<BUILD_INTERFACE:${GLFW_INCLUDE_DIRS}>)

# Conditionally link Vulkan only when using Vulkan renderer
if(BLOCKFORGE_VULKAN_RENDERER)
    target_link_libraries(blockforge PRIVATE vulkan)
endif()
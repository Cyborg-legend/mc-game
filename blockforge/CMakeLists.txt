cmake_minimum_required(VERSION 3.18)
project(BlockForge CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# C++17 features
set(CMAKE_CXX_EXTENSIONS OFF)

# Choose renderer: Vulkan or OpenGL
option(BLOCKFORGE_VULKAN_RENDERER "Use Vulkan renderer" OFF)

# Find Vulkan only if we might use it - make optional
if(BLOCKFORGE_VULKAN_RENDERER)
    find_package(Vulkan REQUIRED)
endif()

find_package(glfw3 REQUIRED)

# External dependencies (header-only)
# nlohmann/json, stb_image, miniaudio are header-only

add_subdirectory(core)
add_subdirectory(renderer)
add_subdirectory(platform)
add_subdirectory(audio)

# assets subdirectory - skip if empty
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/blockforge/assets/CMakeLists.txt")
    # Skip assets if no CMakeLists
endif()

add_executable(blockforge main.cpp)
target_link_libraries(blockforge PRIVATE
    core
    renderer
    platform
)

# Include GLFW headers globally
target_include_directories(blockforge PUBLIC
    $<BUILD_INTERFACE:${GLFW_INCLUDE_DIRS}>)

# Conditionally link Vulkan only when using Vulkan renderer
if(BLOCKFORGE_VULKAN_RENDERER)
    target_link_libraries(blockforge PRIVATE vulkan)
endif()
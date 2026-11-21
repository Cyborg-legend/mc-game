add_library(vulkan STATIC renderer/vulkan/vulkan_context.h renderer/vulkan/vulkan_renderer.h renderer/vulkan/vulkan_renderer.cpp)

target_include_directories(vulkan PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/renderer/vulkan>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/renderer>
    $<BUILD_INTERFACE:${Vulkan_INCLUDE_DIRS}>
)
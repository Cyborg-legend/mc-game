add_library(core STATIC)

# Include parent dirs
target_include_directories(core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/platform/windows>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/platform/linux>
    $<BUILD_INTERFACE:${Vulkan_INCLUDE_DIRS}>
    $<BUILD_INTERFACE:${GLFW_INCLUDE_DIRS}>
)

# ECS module
add_subdirectory(ecs)

# World module
add_subdirectory(world)

# Physics module
add_subdirectory(physics)

# Entities module
add_subdirectory(entities)

# Crafting module
add_subdirectory(crafting)

# Save module
add_subdirectory(save)
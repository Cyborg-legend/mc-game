add_library(renderer STATIC)

# Include parent dirs
target_include_directories(renderer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/renderer>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core>
    $<BUILD_INTERFACE:${GLFW_INCLUDE_DIRS}>
)

# Conditionally include renderer modules based on selection
if(BLOCKFORGE_VULKAN_RENDERER)
    # Vulkan module
    add_subdirectory(vulkan)
    
    # Shaders module (Vulkan-specific)
    add_subdirectory(shaders)
    
    # Meshing module (shared)
    add_subdirectory(meshing)
else
    # OpenGL module
    add_subdirectory(opengl)
    
    # Meshing module (shared - used by both renderers)
    add_subdirectory(meshing)
endif()
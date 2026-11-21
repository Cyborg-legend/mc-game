add_library(opengl STATIC)

# OpenGL 3.3 renderer source files
target_sources(opengl PRIVATE
    opengl_renderer.h
    opengl_renderer.cpp
    shader.h
)

# Include directories
target_include_directories(opengl PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/renderer>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/opengl>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/renderer/meshing>
)

# Link dependencies
target_link_libraries(opengl PUBLIC
    core
    meshing
)

# OpenGL33/GLFW linking
if(WIN32)
    target_link_libraries(opengl PUBLIC opengl32)
elseif(APPLE)
    target_link_libraries(opengl public OpenGL)
else()
    target_link_libraries(opengl PUBLIC GLEW glfw3)
endif()
add_library(renderer STATIC
    meshing/greedy_mesher.cpp
)

target_include_directories(renderer PUBLIC
    ${CMAKE_SOURCE_DIR}/renderer
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/platform/common
    ${CMAKE_SOURCE_DIR}
)

if(BLOCKFORGE_VULKAN_RENDERER)
  add_subdirectory(vulkan)
endif()

add_subdirectory(opengl)   

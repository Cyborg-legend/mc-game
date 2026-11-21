cmake_minimum_required(VERSION 3.18)
project(BlockForge CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(BLOCKFORGE_VULKAN_RENDERER)
  find_package(Vulkan REQUIRED)
endif()
find_package(glfw3 REQUIRED)

add_subdirectory(core)
add_subdirectory(renderer)
add_subdirectory(platform)
add_subdirectory(audio)

add_executable(blockforge main.cpp)
target_include_directories(blockforge PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/renderer
    ${CMAKE_SOURCE_DIR}/platform/common
    ${CMAKE_SOURCE_DIR}/platform/windows
    ${CMAKE_SOURCE_DIR}/platform/linux
    ${CMAKE_SOURCE_DIR}/audio
    ${CMAKE_SOURCE_DIR}/glm_deps
    ${CMAKE_SOURCE_DIR}/nlohmann
)
target_link_libraries(blockforge PRIVATE
    core
    renderer
    platform
    glfw3
)   

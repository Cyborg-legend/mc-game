cmake_minimum_required(VERSION 3.18)
project(BlockForge CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(glfw3 REQUIRED)

file(GLOB_RECURSE ALL_SOURCES ${CMAKE_SOURCE_DIR}/**/*.cpp)
list(REMOVE_ITEM ALL_SOURCES ${CMAKE_SOURCE_DIR}/main.cpp)   
add_library(blockforge_lib STATIC ${ALL_SOURCES})

target_include_directories(blockforge_lib PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/core/ecs
    ${CMAKE_SOURCE_DIR}/core/world
    ${CMAKE_SOURCE_DIR}/core/physics
    ${CMAKE_SOURCE_DIR}/core/entities
    ${CMAKE_SOURCE_DIR}/core/crafting
    ${CMAKE_SOURCE_DIR}/core/save
    ${CMAKE_SOURCE_DIR}/renderer
    ${CMAKE_SOURCE_DIR}/renderer/meshing
    ${CMAKE_SOURCE_DIR}/renderer/opengl
    ${CMAKE_SOURCE_DIR}/platform/common
    ${CMAKE_SOURCE_DIR}/platform/windows
    ${CMAKE_SOURCE_DIR}/platform/linux
    ${CMAKE_SOURCE_DIR}/audio
    ${CMAKE_SOURCE_DIR}/glm_deps
    ${CMAKE_SOURCE_DIR}/glew_deps/include
    ${CMAKE_SOURCE_DIR}/nlohmann
    ${CMAKE_SOURCE_DIR}/glfw_deps/include
)

target_link_libraries(blockforge_lib PUBLIC
    glfw3
    glew32
    opengl32
    gdi32
    user32
)

add_executable(blockforge main.cpp)
target_link_libraries(blockforge PRIVATE blockforge_lib)

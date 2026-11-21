add_library(world STATIC core/world/chunk.h core/world/world.cpp)

target_include_directories(world PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core/world>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core/ecs>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core>
)
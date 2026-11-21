add_library(physics STATIC core/physics/player.cpp core/physics/player.h)

target_include_directories(physics PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core/physics>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core/ecs>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core>
)
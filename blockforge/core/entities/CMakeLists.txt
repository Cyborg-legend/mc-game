add_library(entities STATIC core/entities/player_entity.cpp core/entities/player_entity.h core/entities/mob.cpp core/entities/mob.h)

target_include_directories(entities PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core/entities>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core/ecs>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core/physics>
)
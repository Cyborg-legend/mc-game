add_library(ecs STATIC core/ecs/world.cpp core/ecs/position.h core/ecs/velocity.h core/ecs/health.h core/ecs/mobtype.h core/ecs/playerinput.h core/ecs/entity.h)

target_include_directories(ecs PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core/ecs>
)
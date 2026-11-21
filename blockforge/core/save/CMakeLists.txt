add_library(save STATIC core/save/save.cpp core/save/save.h)

target_include_directories(save PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core/save>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core>
)

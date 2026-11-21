add_library(crafting STATIC core/crafting/recipes.h core/crafting/crafting.cpp)

target_include_directories(crafting PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core/crafting>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core>
)
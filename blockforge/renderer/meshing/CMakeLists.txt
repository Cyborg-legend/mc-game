add_library(meshing STATIC renderer/meshing/mesh.cpp renderer/meshing/mesh.h renderer/meshing/greedy_mesher.h)

target_include_directories(meshing PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/renderer/meshing>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/renderer>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core>
)
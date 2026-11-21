add_library(common STATIC)

target_include_directories(common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/platform/common>
)

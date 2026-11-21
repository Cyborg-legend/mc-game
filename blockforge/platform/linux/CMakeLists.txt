add_library(linux STATIC platform/linux/platform.cpp)

target_include_directories(linux PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/platform/linux>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/platform/common>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/platform/windows>
)
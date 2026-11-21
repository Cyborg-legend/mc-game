add_library(windows STATIC platform/windows/platform.cpp)

target_include_directories(windows PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/platform/windows>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/platform/common>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/platform/linux>
)
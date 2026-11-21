add_library(platform STATIC)

target_include_directories(platform PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/platform>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core>
)

# Windows module
add_subdirectory(windows)

# Linux module
add_subdirectory(linux)

# Common module
add_subdirectory(common)
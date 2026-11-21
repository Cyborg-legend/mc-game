add_library(audio STATIC)

# miniaudio is header-only, just need include dirs
target_include_directories(audio PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/audio>
)
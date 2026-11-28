cmake_minimum_required(VERSION 3.18)
project(BlockForge C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(BFC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

# Dependencies may be unpacked next to the sources (CI) or one level up.
if(EXISTS "${BFC_ROOT}/glfw_deps" OR EXISTS "${BFC_ROOT}/glew_deps")
    set(DEPS_ROOT "${BFC_ROOT}")
else()
    set(DEPS_ROOT "${BFC_ROOT}/..")
endif()

file(GLOB_RECURSE BFC_SOURCES CONFIGURE_DEPENDS "${BFC_ROOT}/src/*.c")

add_executable(blockforge ${BFC_SOURCES})

# GLFW must not pull in a system GL header; GLEW provides the GL entry points.
target_compile_definitions(blockforge PRIVATE GLFW_INCLUDE_NONE GLEW_NO_GLU)
target_include_directories(blockforge PRIVATE
    ${BFC_ROOT}/src
    ${DEPS_ROOT}/glfw_deps/include
    ${DEPS_ROOT}/glew_deps/include
)

# ---- GLFW ----
if(NOT glfw3_DIR AND EXISTS "${DEPS_ROOT}/glfw_deps/glfw3Config.cmake")
    set(glfw3_DIR "${DEPS_ROOT}/glfw_deps")
endif()
find_package(glfw3 CONFIG QUIET)
if(TARGET glfw)
    set(BFC_GLFW_TARGET glfw)
elseif(TARGET glfw3)
    set(BFC_GLFW_TARGET glfw3)
elseif(DEFINED glfw3_LIBRARIES)
    target_include_directories(blockforge PRIVATE ${glfw3_INCLUDE_DIRS})
    set(BFC_GLFW_LIBS ${glfw3_LIBRARIES})
else()
    message(FATAL_ERROR "GLFW not found. Set CMAKE_PREFIX_PATH to the directory containing glfw3Config.cmake")
endif()

# ---- GLEW ----
if(WIN32)
    file(GLOB_RECURSE BFC_GLEW_STATIC_LIBS "${DEPS_ROOT}/glew_deps/*glew32s.lib")
    file(GLOB_RECURSE BFC_GLEW_SHARED_LIBS "${DEPS_ROOT}/glew_deps/*glew32.lib")
    if(BFC_GLEW_STATIC_LIBS)
        list(GET BFC_GLEW_STATIC_LIBS 0 BFC_GLEW_LIB)
        target_compile_definitions(blockforge PRIVATE GLEW_STATIC)
    elseif(BFC_GLEW_SHARED_LIBS)
        list(GET BFC_GLEW_SHARED_LIBS 0 BFC_GLEW_LIB)
    else()
        message(FATAL_ERROR "GLEW import library not found under ${DEPS_ROOT}/glew_deps")
    endif()
else()
    find_package(GLEW QUIET)
    if(TARGET GLEW::GLEW)
        set(BFC_GLEW_LIB GLEW::GLEW)
    elseif(GLEW_FOUND AND GLEW_LIBRARIES)
        set(BFC_GLEW_LIB ${GLEW_LIBRARIES})
    else()
        set(BFC_GLEW_LIB GL)
    endif()
endif()

target_link_libraries(blockforge PRIVATE ${BFC_GLEW_LIB})
if(BFC_GLFW_TARGET)
    target_link_libraries(blockforge PRIVATE ${BFC_GLFW_TARGET})
else()
    target_link_libraries(blockforge PRIVATE ${BFC_GLFW_LIBS})
endif()

if(WIN32)
    # winmm provides the waveOut mixer that src/audio.c drives.
    target_link_libraries(blockforge PRIVATE opengl32 gdi32 user32 shell32 winmm)
else()
    find_package(OpenGL QUIET)
    if(TARGET OpenGL::GL)
        target_link_libraries(blockforge PRIVATE OpenGL::GL ${CMAKE_DL_LIBS} pthread m)
    else()
        message(WARNING "OpenGL development libraries not found; install libgl-dev for a local Linux build")
        target_link_libraries(blockforge PRIVATE GL ${CMAKE_DL_LIBS} pthread m)
    endif()
endif()

if(MSVC)
    target_compile_options(blockforge PRIVATE /W4)
else()
    target_compile_options(blockforge PRIVATE -Wall -Wextra)
endif()

# Headless checks: pure C, no window or GPU needed. Build with
# -DBUILD_TESTING=ON (the default) and run them with ctest.
include(CTest)
if(BUILD_TESTING)
    add_executable(check_shader_variation
        ${BFC_ROOT}/tests/check_shader_variation.c
        ${BFC_ROOT}/src/shaders.c
    )
    target_include_directories(check_shader_variation PRIVATE ${BFC_ROOT}/src)
    add_test(NAME shader_variation COMMAND check_shader_variation)
endif()

# Copy runtime DLLs next to the executable so the build output is runnable.
if(WIN32)
    file(GLOB_RECURSE BFC_RUNTIME_DLLS
        "${DEPS_ROOT}/glew_deps/*.dll"
        "${DEPS_ROOT}/glfw_deps/*.dll")
    foreach(dll ${BFC_RUNTIME_DLLS})
        add_custom_command(TARGET blockforge POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll}" "$<TARGET_FILE_DIR:blockforge>")
    endforeach()
endif()

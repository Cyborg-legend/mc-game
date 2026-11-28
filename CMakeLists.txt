cmake_minimum_required(VERSION 3.18)
project(BlockForge C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(BFC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

# Experimental: build chunk terrain on one POSIX worker thread and hand the
# block arrays back to the main thread for lighting, decoration and meshing.
# Off by default; the synchronous path stays the supported one.
option(BLOCKFORGE_ASYNC_CHUNKS "Generate chunk terrain on a worker thread (POSIX only)" OFF)
if(BLOCKFORGE_ASYNC_CHUNKS)
    find_package(Threads REQUIRED)
endif()

# The worker header is GL-free, so its queue check always builds. The worker
# itself only links when the switch is on.
add_library(blockforge_chunkqueue STATIC ${BFC_ROOT}/src/chunkqueue.c)
target_include_directories(blockforge_chunkqueue PUBLIC ${BFC_ROOT}/src)

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
if(BLOCKFORGE_ASYNC_CHUNKS)
    target_compile_definitions(blockforge PRIVATE BG_ASYNC_CHUNKS)
    target_link_libraries(blockforge PRIVATE Threads::Threads blockforge_chunkqueue)
endif()
target_include_directories(blockforge PRIVATE
    ${BFC_ROOT}/src
    ${DEPS_ROOT}/glfw_deps/include
    ${DEPS_ROOT}/glew_deps/include
)

# ---- GLFW ----
# Three sources, in order: the CMake config (the Windows CI download, or a
# distro package such as Ubuntu's libglfw3-dev), pkg-config (older distros
# shipped only a .pc file), then a plain header+library search. Any of them
# is enough, so the checks job does not hinge on one packaging choice.
if(NOT glfw3_DIR AND EXISTS "${DEPS_ROOT}/glfw_deps/glfw3Config.cmake")
    set(glfw3_DIR "${DEPS_ROOT}/glfw_deps")
endif()
find_package(glfw3 CONFIG QUIET)

set(BFC_GLFW_TARGET "")
set(BFC_GLFW_LIBS "")
set(BFC_GLFW_INCLUDES "")

# Package configs across versions export different target names.
foreach(candidate glfw glfw3 glfw3::glfw GLFW::GLFW)
    if(NOT BFC_GLFW_TARGET AND TARGET ${candidate})
        set(BFC_GLFW_TARGET ${candidate})
    endif()
endforeach()

# Some configs (including the generated one in CI) only set variables.
if(NOT BFC_GLFW_TARGET AND DEFINED glfw3_LIBRARIES)
    set(BFC_GLFW_LIBS ${glfw3_LIBRARIES})
    set(BFC_GLFW_INCLUDES ${glfw3_INCLUDE_DIRS})
endif()

if(NOT BFC_GLFW_TARGET AND NOT BFC_GLFW_LIBS)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(BFC_GLFW_PC QUIET IMPORTED_TARGET glfw3)
        if(BFC_GLFW_PC_FOUND)
            set(BFC_GLFW_TARGET PkgConfig::BFC_GLFW_PC)
        endif()
    endif()
endif()

if(NOT BFC_GLFW_TARGET AND NOT BFC_GLFW_LIBS)
    find_path(BFC_GLFW_INCLUDE_DIR GLFW/glfw3.h)
    find_library(BFC_GLFW_LIBRARY NAMES glfw glfw3)
    if(BFC_GLFW_INCLUDE_DIR AND BFC_GLFW_LIBRARY)
        set(BFC_GLFW_LIBS ${BFC_GLFW_LIBRARY})
        set(BFC_GLFW_INCLUDES ${BFC_GLFW_INCLUDE_DIR})
    endif()
endif()

if(NOT BFC_GLFW_TARGET AND NOT BFC_GLFW_LIBS)
    message(FATAL_ERROR
        "GLFW not found. Install libglfw3-dev (Debian/Ubuntu) or set "
        "CMAKE_PREFIX_PATH to the directory containing glfw3Config.cmake")
endif()

if(BFC_GLFW_INCLUDES)
    target_include_directories(blockforge PRIVATE ${BFC_GLFW_INCLUDES})
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
    # libm is a separate library only on Unix. Windows gets the math functions
    # from the CRT, and asking for "m" there makes the linker look for m.lib,
    # which does not exist (LNK1181). So the checks share one list that is
    # empty on Windows and "m" everywhere else.
    set(BFC_CHECK_LIBS "")
    if(NOT WIN32)
        set(BFC_CHECK_LIBS m)
    endif()

    # Every check is plain C that needs no window, GPU or game state, so they
    # share the same include path and math library setup.
    function(bfc_check name)
        target_include_directories(${name} PRIVATE ${BFC_ROOT}/src)
        if(BFC_CHECK_LIBS)
            target_link_libraries(${name} PRIVATE ${BFC_CHECK_LIBS})
        endif()
    endfunction()

    add_executable(check_shader_variation
        ${BFC_ROOT}/tests/check_shader_variation.c
        ${BFC_ROOT}/src/shaders.c
    )
    bfc_check(check_shader_variation)
    add_test(NAME shader_variation COMMAND check_shader_variation)

    add_executable(check_timestep
        ${BFC_ROOT}/tests/check_timestep.c
        ${BFC_ROOT}/src/timestep.c
    )
    bfc_check(check_timestep)
    add_test(NAME timestep COMMAND check_timestep)

    add_executable(check_interp ${BFC_ROOT}/tests/check_interp.c)
    bfc_check(check_interp)
    add_test(NAME interpolation COMMAND check_interp)

    add_executable(check_effects ${BFC_ROOT}/tests/check_effects.c)
    bfc_check(check_effects)
    add_test(NAME effects COMMAND check_effects)

    add_executable(check_framecap
        ${BFC_ROOT}/tests/check_framecap.c
        ${BFC_ROOT}/src/framecap.c
    )
    bfc_check(check_framecap)
    add_test(NAME framecap COMMAND check_framecap)

    add_executable(check_worldmeta
        ${BFC_ROOT}/tests/check_worldmeta.c
        ${BFC_ROOT}/src/worldmeta.c
    )
    bfc_check(check_worldmeta)
    add_test(NAME worldmeta COMMAND check_worldmeta)

    add_executable(check_gltrace
        ${BFC_ROOT}/tests/check_gltrace.c
        ${BFC_ROOT}/src/gltrace.c
    )
    bfc_check(check_gltrace)
    add_test(NAME gltrace COMMAND check_gltrace)

    add_executable(check_loot
        ${BFC_ROOT}/tests/check_loot.c
        ${BFC_ROOT}/src/loot.c
    )
    bfc_check(check_loot)
    add_test(NAME loot COMMAND check_loot)

    add_executable(check_chunkqueue ${BFC_ROOT}/tests/check_chunkqueue.c)
    bfc_check(check_chunkqueue)
    target_link_libraries(check_chunkqueue PRIVATE blockforge_chunkqueue)
    add_test(NAME chunkqueue COMMAND check_chunkqueue)

    # Real threads, so only registered when the experimental path is built.
    if(BLOCKFORGE_ASYNC_CHUNKS)
        add_executable(check_chunkworker
            ${BFC_ROOT}/tests/check_chunkworker.c
            ${BFC_ROOT}/src/chunkworker.c
        )
        bfc_check(check_chunkworker)
        target_compile_definitions(check_chunkworker PRIVATE BG_ASYNC_CHUNKS)
        target_link_libraries(check_chunkworker PRIVATE Threads::Threads blockforge_chunkqueue)
        add_test(NAME chunkworker COMMAND check_chunkworker)
    endif()
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

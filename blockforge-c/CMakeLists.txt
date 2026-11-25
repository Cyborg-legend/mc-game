cmake_minimum_required(VERSION 3.18)
project(BlockForgeC C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(BFC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(DEPS_ROOT "${BFC_ROOT}/..")

file(GLOB_RECURSE BFC_SOURCES CONFIGURE_DEPENDS "${BFC_ROOT}/src/*.c")

add_executable(blockforge-c ${BFC_SOURCES})

# GLFW must not pull in a system GL header; GLEW provides the GL entry points.
target_compile_definitions(blockforge-c PRIVATE GLFW_INCLUDE_NONE GLEW_NO_GLU)
target_include_directories(blockforge-c PRIVATE
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
    target_include_directories(blockforge-c PRIVATE ${glfw3_INCLUDE_DIRS})
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
        target_compile_definitions(blockforge-c PRIVATE GLEW_STATIC)
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

target_link_libraries(blockforge-c PRIVATE ${BFC_GLEW_LIB})
if(BFC_GLFW_TARGET)
    target_link_libraries(blockforge-c PRIVATE ${BFC_GLFW_TARGET})
else()
    target_link_libraries(blockforge-c PRIVATE ${BFC_GLFW_LIBS})
endif()

if(WIN32)
    target_link_libraries(blockforge-c PRIVATE opengl32 gdi32 user32 shell32)
else()
    find_package(OpenGL QUIET)
    if(TARGET OpenGL::GL)
        target_link_libraries(blockforge-c PRIVATE OpenGL::GL ${CMAKE_DL_LIBS} pthread m)
    else()
        message(WARNING "OpenGL development libraries not found; install libgl-dev for a local Linux build")
        target_link_libraries(blockforge-c PRIVATE GL ${CMAKE_DL_LIBS} pthread m)
    endif()
endif()

if(MSVC)
    target_compile_options(blockforge-c PRIVATE /W4)
else()
    target_compile_options(blockforge-c PRIVATE -Wall -Wextra)
endif()

# Copy runtime DLLs next to the executable so the build output is runnable.
if(WIN32)
    file(GLOB_RECURSE BFC_RUNTIME_DLLS
        "${DEPS_ROOT}/glew_deps/*.dll"
        "${DEPS_ROOT}/glfw_deps/*.dll")
    foreach(dll ${BFC_RUNTIME_DLLS})
        add_custom_command(TARGET blockforge-c POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll}" "$<TARGET_FILE_DIR:blockforge-c>")
    endforeach()
endif()

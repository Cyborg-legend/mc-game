cmake_minimum_required(VERSION 3.18)
project(BlockForge CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(BF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(DEPS_ROOT "${BF_ROOT}/..")

# ---- Sources (recursive, single flat build) ----
file(GLOB_RECURSE ALL_SOURCES CONFIGURE_DEPENDS "${BF_ROOT}/*.cpp")
if(WIN32)
    list(FILTER ALL_SOURCES EXCLUDE REGEX "/platform/linux/")
else()
    list(FILTER ALL_SOURCES EXCLUDE REGEX "/platform/windows/")
endif()
list(REMOVE_ITEM ALL_SOURCES "${BF_ROOT}/main.cpp")

add_library(blockforge_lib STATIC ${ALL_SOURCES})

# GLFW must not pull in a system GL header; GLEW provides the GL entry points.
target_compile_definitions(blockforge_lib PUBLIC GLFW_INCLUDE_NONE GLEW_NO_GLU)

target_include_directories(blockforge_lib PUBLIC
    ${BF_ROOT}
    ${BF_ROOT}/core
    ${BF_ROOT}/renderer
    ${BF_ROOT}/platform
    ${BF_ROOT}/audio
    ${BF_ROOT}/debug
    ${DEPS_ROOT}
    ${DEPS_ROOT}/nlohmann
    ${DEPS_ROOT}/glm_deps
    ${DEPS_ROOT}/glfw_deps/include
    ${DEPS_ROOT}/glew_deps/include
)

# ---- GLFW ----
if(NOT glfw3_DIR AND EXISTS "${DEPS_ROOT}/glfw_deps/glfw3Config.cmake")
    set(glfw3_DIR "${DEPS_ROOT}/glfw_deps")
endif()
find_package(glfw3 CONFIG QUIET)
if(TARGET glfw)
    set(BF_GLFW_TARGET glfw)
elseif(TARGET glfw3)
    set(BF_GLFW_TARGET glfw3)
elseif(DEFINED glfw3_LIBRARIES)
    target_include_directories(blockforge_lib PUBLIC ${glfw3_INCLUDE_DIRS})
    set(BF_GLFW_LIBS ${glfw3_LIBRARIES})
else()
    message(FATAL_ERROR "GLFW not found. Set CMAKE_PREFIX_PATH to the directory containing glfw3Config.cmake")
endif()

# ---- GLEW ----
if(WIN32)
    file(GLOB_RECURSE BF_GLEW_STATIC_LIBS "${DEPS_ROOT}/glew_deps/*glew32s.lib")
    file(GLOB_RECURSE BF_GLEW_SHARED_LIBS "${DEPS_ROOT}/glew_deps/*glew32.lib")
    if(BF_GLEW_STATIC_LIBS)
        list(GET BF_GLEW_STATIC_LIBS 0 BF_GLEW_LIB)
        target_compile_definitions(blockforge_lib PUBLIC GLEW_STATIC)
    elseif(BF_GLEW_SHARED_LIBS)
        list(GET BF_GLEW_SHARED_LIBS 0 BF_GLEW_LIB)
    else()
        message(FATAL_ERROR "GLEW import library not found under ${DEPS_ROOT}/glew_deps")
    endif()
else()
    find_package(GLEW QUIET)
    if(TARGET GLEW::GLEW)
        set(BF_GLEW_LIB GLEW::GLEW)
    elseif(GLEW_FOUND AND GLEW_LIBRARIES)
        set(BF_GLEW_LIB ${GLEW_LIBRARIES})
    else()
        set(BF_GLEW_LIB GL)
    endif()
endif()

target_link_libraries(blockforge_lib PUBLIC ${BF_GLEW_LIB})
if(BF_GLFW_TARGET)
    target_link_libraries(blockforge_lib PUBLIC ${BF_GLFW_TARGET})
else()
    target_link_libraries(blockforge_lib PUBLIC ${BF_GLFW_LIBS})
endif()

if(WIN32)
    target_link_libraries(blockforge_lib PUBLIC opengl32 gdi32 user32 shell32)
else()
    find_package(OpenGL QUIET)
    if(TARGET OpenGL::GL)
        target_link_libraries(blockforge_lib PUBLIC OpenGL::GL ${CMAKE_DL_LIBS} pthread)
    else()
        message(WARNING "OpenGL development libraries not found; install libgl-dev for a local Linux build")
        target_link_libraries(blockforge_lib PUBLIC GL ${CMAKE_DL_LIBS} pthread)
    endif()
endif()

# ---- Executable ----
add_executable(blockforge "${BF_ROOT}/main.cpp")
target_link_libraries(blockforge PRIVATE blockforge_lib)

# Copy runtime DLLs next to the executable so the build output is runnable.
if(WIN32)
    file(GLOB_RECURSE BF_RUNTIME_DLLS
        "${DEPS_ROOT}/glew_deps/*.dll"
        "${DEPS_ROOT}/glfw_deps/*.dll")
    foreach(dll ${BF_RUNTIME_DLLS})
        add_custom_command(TARGET blockforge POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll}" "$<TARGET_FILE_DIR:blockforge>")
    endforeach()
endif()

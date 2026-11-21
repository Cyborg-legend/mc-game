add_library(shaders STATIC)

# OpenGL shaders - vertex and fragment for the OpenGL renderer
# These are embedded as source strings in the code, not separate files
set(SHADER_SOURCES STATIC)

# The shaders are included via #include in the renderer code
# No separate file processing needed for OpenGL
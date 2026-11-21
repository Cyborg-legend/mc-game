add_library(shaders STATIC)

# Shader files - will be copied or embedded
set(SHADER_SOURCES
    shader.opvs
    shader.frag
)

# Add custom command to handle shaders
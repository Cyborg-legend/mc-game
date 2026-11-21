#version 330 core

// Vertex shader for chunk rendering
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in float aAO;

// Output to fragment shader
out vec3 WorldNormal;
out vec2 TexCoords;
out float AO;

// Uniforms
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 lightPos;
uniform vec3 viewPos;

// Material
uniform float shininess = 32.0;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    
    // Pass normal and UV to fragment shader
    WorldNormal = normalize(mat3(model) * aNormal);
    TexCoords = aUV;
    AO = aAO;
}
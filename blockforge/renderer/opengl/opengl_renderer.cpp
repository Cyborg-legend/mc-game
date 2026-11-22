#include "opengl_renderer.h"
#include "meshing/mesh.h"
#include "core/world/chunk.h"
#include <glm/glm.hpp>
#include <iostream>
#include <string>

// ============================================================
// Shader implementation - uses embedded source strings
// ============================================================

Shader::Shader(const char* vertexSource, const char* fragmentSource)
    : vertexSource_(vertexSource), fragmentSource_(fragmentSource)
{
    unsigned int vertex, fragment;
    int success;
    char infoLog[512];

    // Compile vertex shader
    vertex = compileShader(GL_VERTEX_SHADER, std::string(vertexSource_));
    if (!vertex) {
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED" << std::endl;
        return;
    }

    // Compile fragment shader
    fragment = compileShader(GL_FRAGMENT_SHADER, std::string(fragmentSource_));
    if (!fragment) {
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED" << std::endl;
        return;
    }

    // Link program
    ID = glCreateProgram();
    glAttachShader(ID, vertex);
    glAttachShader(ID, fragment);
    glLinkProgram(ID);

    glGetProgramiv(ID, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(ID, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    // Delete shaders as they're linked into our program now
    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

Shader::~Shader() {
    if (ID) glDeleteProgram(ID);
}

unsigned int Shader::compileShader(unsigned int type, const std::string& source) {
    unsigned int shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    return shader;
}

// ============================================================
// OpenGLRenderer implementation
// ============================================================

// Embedded shader source code - vertex shader
static const char* kVertexShaderSource = "#version 330 core\n"
// Attributes: position, normal, uv, ao
"layout(location = 0) in vec3 aPos;\n"
"layout(location = 1) in vec3 aNormal;\n"
"layout(location = 2) in vec2 aUV;\n"
"layout(location = 3) in float aAO;\n"
// Output to fragment shader
"out vec3 WorldNormal;\n"
"out vec2 TexCoords;\n"
"out float AO;\n"
// Uniforms
"uniform mat4 model;\n"
"uniform mat4 view;\n"
"uniform mat4 projection;\n"
"uniform vec3 lightPos;\n"
"uniform vec3 viewPos;\n"
// Material
"uniform float shininess = 32.0;\n"
"void main()\n"
"{\n"
"    gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
// Pass normal and UV to fragment shader
"    WorldNormal = normalize(mat3(model) * aNormal);\n"
"    TexCoords = aUV;\n"
"    AO = aAO;\n"
"}";

// Embedded shader source code - fragment shader
static const char* kFragmentShaderSource = "#version 330 core\n"
// Texture sampling, directional light + ambient, AO modulation
"in vec3 WorldNormal;\n"
"in vec2 TexCoords;\n"
"in float AO;\n"
// Output
"out vec4 FragColor;\n"
// Texture sampler for the 256x256 atlas
"uniform sampler2D texAtlas;\n"
// Material
"uniform vec3 lightPos;\n"
"uniform vec3 viewPos;\n"
"uniform float shininess = 32.0;\n"
// Block type colors (approximate)
"struct BlockColor {\n"
"    vec3 ambient;\n"
"    vec3 diffuse;\n"
"    vec3 specular;\n"
"};\n"
// 16-block color palette
"const BlockColor blockColors[16] = {\n"
"    [0] = {vec3(0.0), vec3(0.0), vec3(0.0)},       // Air - invisible\n"
"    [1] = {vec3(0.1), vec3(0.5, 0.5, 0.5), vec3(0.0)},   // Stone\n"
"    [2] = {vec3(0.1), vec3(0.6, 0.4, 0.2), vec3(0.0)},   // Dirt\n"
"    [3] = {vec3(0.1), vec3(0.1, 0.8, 0.1), vec3(0.0)},   // Grass\n"
"    [4] = {vec3(0.1), vec3(0.5, 0.3, 0.1), vec3(0.1)},   // Wood\n"
"    [5] = {vec3(0.1), vec3(0.2, 0.7, 0.2), vec3(0.0)},   // Leaves\n"
"    [6] = {vec3(0.1), vec3(0.8, 0.8, 0.2), vec3(0.0)},   // Sand\n"
"    [7] = {vec3(0.1), vec3(0.1, 0.5, 0.8), vec3(0.0)},   // Water\n"
"    [8] = {vec3(0.1), vec3(0.3, 0.3, 0.3), vec3(0.0)},   // Bedrock\n"
"    [9] = {vec3(0.1), vec3(0.7, 0.6, 0.4), vec3(0.1)},   // Planks\n"
"    [10] = {vec3(0.1), vec3(0.6, 0.6, 0.6), vec3(0.0)},  // Sticks\n"
"    [11] = {vec3(0.1), vec3(0.8, 0.6, 0.3), vec3(0.1)},  // CraftingTable\n"
"    [12] = {vec3(0.1), vec3(0.7, 0.3, 0.3), vec3(0.1)},  // Brick\n"
"    [13] = {vec3(0.0), vec3(0.0), vec3(0.0)},  // placeholder\n"
"    [14] = {vec3(0.0), vec3(0.0), vec3(0.0)},  // placeholder\n"
"    [15] = {vec3(0.0), vec3(0.0), vec3(0.0)}   // Count\n"
"};\n"
"void main()\n"
"{\n"
// Get block type from texture coordinate (first channel)
"    vec4 texColor = texture(texAtlas, TexCoords);\n"
// Use the block type index from RGB
"    int blockType = int(texColor.r * 15.0); // assuming type index 0-15 packed in r\n"
// Clamp to valid range
"    if (blockType < 0) blockType = 0;\n"
"    if (blockType >= 16) blockType = 15;\n"
// Get block color
"    BlockColor bc = blockColors[blockType];\n"
// Ambient
"    vec3 ambient = bc.ambient;\n"
// Diffuse lighting (Lambert)
"    vec3 normal = normalize(WorldNormal);\n"
"    vec3 lightDir = normalize(lightPos - viewPos);\n"
"    float diff = max(dot(normal, lightDir), 0.0);\n"
"    vec3 diffuse = bc.diffuse * diff;\n"
// Specular (Blinn-Phong)
"    vec3 viewDir = normalize(viewPos - vec3(0.0)); // Simplified\n"
"    float spec = pow(max(dot(viewDir, normal), 0.0), shininess);\n"
"    vec3 specular = bc.specular * spec;\n"
// Combine
"    vec3 result = ambient + diffuse + specular;\n"
// Apply AO
"    result *= AO;\n"
// Water transparency
"    if (blockType == 7) { // Water\n"
"        result *= vec3(0.5); // Dim water\n"
"        FragColor = vec4(result, 0.6); // Semi-transparent\n"
"    } else {\n"
"        FragColor = vec4(result, 1.0);\n"
"    }\n"} ;

Shader::Shader() 
    : vertexSource_(kVertexShaderSource), fragmentSource_(kFragmentShaderSource)
{
    // Initialize with embedded sources
    unsigned int vertex, fragment;
    int success;
    char infoLog[512];

    // Compile vertex shader
    vertex = compileShader(GL_VERTEX_SHADER, std::string(vertexSource_));
    if (!vertex) {
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED" << std::endl;
        return;
    }

    // Compile fragment shader
    fragment = compileShader(GL_FRAGMENT_SHADER, std::string(fragmentSource_));
    if (!fragment) {
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED" << std::endl;
        return;
    }

    // Link program
    ID = glCreateProgram();
    glAttachShader(ID, vertex);
    glAttachShader(ID, fragment);
    glLinkProgram(ID);

    glGetProgramiv(ID, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(ID, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    // Delete shaders as they're linked into our program now
    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

OpenGLRenderer::OpenGLRenderer()
    : shader_(kVertexShaderSource, kFragmentShaderSource),
      vao_(0), vbo_(0), ibo_(0), textureAtlas_(0),
      lightPos_(0.0f, 20.0f, 0.0f), renderDistance_(16) {}

OpenGLRenderer::~OpenGLRenderer() {
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (ibo_) glDeleteBuffers(1, &ibo_);
}

bool OpenGLRenderer::init() {
    // GLEW is initialized elsewhere (in main.cpp), just ensure we can use it
    // Check OpenGL version
    const char* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    std::cout << "OpenGL Version: " << glVersion << std::endl;
    const char* glslVersion = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
    std::cout << "GLSL Version: " << glslVersion << std::endl;

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Enable cull face
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Setup VAO/VBO for chunk meshes
    initMeshBuffers();

    // Set light position
    shader_.setVec3("lightPos", lightPos_.x, lightPos_.y, lightPos_.z);

    return true;
}

void OpenGLRenderer::initMeshBuffers() {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ibo_);

    glBindVertexArray(vao_);

    // Vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    // Index buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);

    // Vertex attribute pointers (setup once, data updated per chunk)
    // Position (3 floats)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    // Normal (3 floats)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    // UV (2 floats)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));

    // AO (1 float)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, ao));

    glBindVertexArray(0);
}

void OpenGLRenderer::uploadChunk(uint64_t id, const MeshChunk& mesh) {
    uploadedChunks_[id] = mesh;

    if (mesh.vertices.empty() || mesh.indices.empty()) return;

    // Update VBO with mesh data
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
        mesh.vertices.size() * sizeof(Vertex),
        mesh.vertices.data(),
        GL_DYNAMIC_DRAW);

    // Update IBO with index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        mesh.indices.size() * sizeof(uint32_t),
        mesh.indices.data(),
        GL_DYNAMIC_DRAW);
}

void OpenGLRenderer::destroyChunk(uint64_t id) {
    uploadedChunks_.erase(id);
    // No explicit GPU cleanup needed - VAO/VBO are shared
}

bool OpenGLRenderer::isChunkVisible(uint64_t chunkKey, const Camera& cam) {
    // Unpack chunk coordinates
    int cx, cy, cz;
    // World::UnpackCoords(chunkKey, cx, cy, cz); // Simplified

    // Chunk AABB: 16×16×16 blocks, each block is 1 unit
    // For now, use a simple distance check
    // In a full implementation, this would be a 6-plane frustum test

    // Get chunk center (assuming chunk 0,0,0 for simplicity)
    float cxf = 8.0f; // chunk center X
    float cyf = 8.0f; // chunk center Y
    float czf = 8.0f; // chunk center Z

    // Distance from camera to chunk center
    float dx = cxf - cam.position.x;
    float dy = cyf - cam.position.y;
    float dz = czf - cam.position.z;
    float distSq = dx * dx + dy * dy + dz * dz;

    // Simple distance-based culling
    // Visible if within render distance
    float maxDist = renderDistance_ * 16.0f + 8.0f;
    return distSq < maxDist * maxDist;
}

void OpenGLRenderer::render(const Camera& cam, const std::vector<uint64_t>& visibleChunks) {
    if (!shader_.ID) {
        std::cerr << "Shader not initialized!" << std::endl;
        return;
    }

    shader_.use();

    // Set projection matrix
    shader_.setMat4("projection", cam.projection);

    // Set view matrix
    shader_.setMat4("view", cam.getViewMatrix());

    // Set camera position
    shader_.setVec3("viewPos", cam.position.x, cam.position.y, cam.position.z);

    // Set light position
    shader_.setVec3("lightPos", lightPos_.x, lightPos_.y, lightPos_.z);

    // Set render distance uniform
    shader_.setInt("renderDistance", renderDistance_);

    // Bind VAO
    glBindVertexArray(vao_);

    // Render each visible chunk
    for (uint64_t chunkKey : visibleChunks) {
        auto it = uploadedChunks_.find(chunkKey);
        if (it == uploadedChunks_.end()) continue;

        const MeshChunk& mesh = it->second;

        if (mesh.vertices.empty() || mesh.indices.empty()) continue;

        // Update VBO/IBO with current chunk data
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER,
            mesh.vertices.size() * sizeof(Vertex),
            mesh.vertices.data(),
            GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            mesh.indices.size() * sizeof(uint32_t),
            mesh.indices.data(),
            GL_DYNAMIC_DRAW);

        // Draw the chunk
        glDrawElements(GL_TRIANGLES,
            mesh.indexCount,
            GL_UNSIGNED_INT,
            0);
    }

    // Unbind
    glBindVertexArray(0);
}

void OpenGLRenderer::setTextureAtlas(GLuint atlas) {
    textureAtlas_ = atlas;
}

void OpenGLRenderer::setLightPosition(const glm::vec3& pos) {
    lightPos_ = pos;
    shader_.setVec3("lightPos", pos.x, pos.y, pos.z);
}

void OpenGLRenderer::updateRenderDistance(float frameTime) {
    renderDistanceTimer_ += static_cast<int>(frameTime * 1000.0f); // Convert to ms

    if (renderDistanceTimer_ >= 10000) { // 10 seconds
        // Check average frame time - if < 16ms, increase; if > 33ms, decrease
        if (lastFrameTime_ < 16.0f / 1000.0f) {
            // Increase render distance
            if (renderDistance_ < 64) {
                renderDistance_ += 2;
            }
        }
        else if (lastFrameTime_ > 33.0f / 1000.0f) {
            // Decrease render distance
            if (renderDistance_ > 8) {
                renderDistance_ -= 2;
            }
        }
        renderDistanceTimer_ = 0;
    }
    // Clamp range: 8-64
    if (renderDistance_ < 8) renderDistance_ = 8;
    if (renderDistance_ > 64) renderDistance_ = 64;
}
#include "opengl_renderer.h"
#include "meshing/mesh.h"
#include "core/world/chunk.h"
#include <glm/glm.hpp>
#include <iostream>
#include <fstream>

// ============================================================
// Shader implementation
// ============================================================

Shader::Shader(const char* vertexPath, const char* fragmentPath)
    : vertexPath(vertexPath), fragmentPath(fragmentPath) {
    // Read vertex shader
    std::string vertexCode;
    std::ifstream vShaderFile;
    vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::eofbit);
    try {
        vShaderFile.open(vertexPath);
        std::stringstream vShaderStream;
        vShaderStream << vShaderFile.rdbuf();
        vertexCode = vShaderStream.str();
        vShaderFile.close();
    }
    catch (std::exception& e) {
        std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ: " << vertexPath << std::endl;
    }

    // Read fragment shader
    std::string fragmentCode;
    std::ifstream fShaderFile;
    fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::eofbit);
    try {
        fShaderFile.open(fragmentPath);
        std::stringstream fShaderStream;
        fShaderStream << fShaderFile.rdbuf();
        fragmentCode = fShaderStream.str();
        fShaderFile.close();
    }
    catch (std::exception& e) {
        std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ: " << fragmentPath << std::endl;
    }

    // Compile shaders
    unsigned int vertex, fragment;
    int success;
    char infoLog[512];

    // Vertex shader
    vertex = compileShader(GL_VERTEX_SHADER, vertexCode);
    if (!vertex) {
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED" << std::endl;
        return;
    }

    // Fragment shader
    fragment = compileShader(GL_FRAGMENT_SHADER, fragmentCode);
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

    // Delete shaders as they're linked into our program now and no longer necessery
    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

void Shader::use() {
    glUseProgram(ID);
}

void Shader::setMat4(const std::string& name, const glm::mat4& mat) {
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}

void Shader::setVec3(const std::string& name, float v0, float v1, float v2) {
    glUniform3f(glGetUniformLocation(ID, name.c_str()), v0, v1, v2);
}

void Shader::setVec3(const std::string& name, const glm::vec3& vec) {
    glUniform3f(glGetUniformLocation(ID, name.c_str()), vec.x, vec.y, vec.z);
}

void Shader::setFloat(const std::string& name, float value) {
    glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setInt(const std::string& name, int value) {
    glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
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

Shader::~Shader() {
    glDeleteProgram(ID);
}

// ============================================================
// OpenGLRenderer implementation
// ============================================================

OpenGLRenderer::OpenGLRenderer()
    : shader_("blockforge/opengl/shaders/vertex.glsl", "blockforge/opengl/shaders/fragment.glsl"),
      vao_(0), vbo_(0), ibo_(0), textureAtlas_(0),
      lightPos_(0.0f, 20.0f, 0.0f), renderDistance_(16) {}

OpenGLRenderer::~OpenGLRenderer() {
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (ibo_) glDeleteBuffers(1, &ibo_);
}

bool OpenGLRenderer::init() {
    // Initialize GLEW
    GLenum glewError = glewInit();
    if (glewError != GLEW_OK) {
        std::cerr << "Error initializing GLEW: " << glewGetErrorString(glewError) << std::endl;
        return false;
    }

    // Check OpenGL version
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

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
    // We'll set the buffer size dynamically based on mesh data

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

    // Update VBO with mesh data
    if (mesh.vertices.empty()) return;

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, 
                 mesh.vertices.size() * sizeof(Vertex), 
                 mesh.vertices.data(), 
                 GL_DYNAMIC_DRAW);

    // Update IBO with index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(GL_ELEMENT_BUFFER_ARRAY_BUFFER, 
                 mesh.indices.size() * sizeof(uint32_t), 
                 mesh.indices.data(), 
                 GL_DYNAMIC_DRAW);
}

void OpenGLRenderer::destroyChunk(uint64_t id) {
    uploadedChunks_.erase(id);
    // No explicit GPU cleanup needed for now - VAO/VBO are shared
}

bool OpenGLRenderer::isChunkVisible(uint64_t chunkKey, const Camera& cam) {
    // Unpack chunk coordinates
    int cx, cy, cz;
    World::UnpackCoords(chunkKey, cx, cy, cz);

    // Chunk AABB: 16×16×16 blocks, each block is 1 unit
    // Chunk center at (cx*16+8, cy*16+8, cz*16+8)
    float cxf = static_cast<float>(cx * 16 + 8);
    float cyf = static_cast<float>(cy * 16 + 8);
    float czf = static_cast<float>(cz * 16 + 8);
    float extent = 8.0f;

    // Get view matrix and compute camera position in world space
    glm::mat4 view = cam.getViewMatrix();
    glm::vec3 camPos = cam.position;

    // Distance from camera to chunk center
    float dx = cxf - camPos.x;
    float dy = cyf - camPos.y;
    float dz = czf - camPos.z;
    float dist = dx * dx + dy * dy + dz * dz;

    // Simple distance-based culling (placeholder for real frustum culling)
    // Visible if within render distance
    float maxDist = renderDistance_ * 16.0f + extent;
    return dist < maxDist * maxDist;
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

    // Set texture atlas (bound elsewhere)
    // glActiveTexture(GL_TEXTURE0);
    // glBindTexture(GL_TEXTURE_2D, textureAtlas_);

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
        glBufferData(GL_ELEMENT_BUFFER_ARRAY_BUFFER, 
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
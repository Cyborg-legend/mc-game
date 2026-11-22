#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <cstdint>
#include <array>
#include <map>
#include <string>

// GLEW for OpenGL function loading
#include <GL/glew.h>

// Vertex format for mesh
struct Vertex {
    glm::vec3 pos;     // Position (3 floats)
    glm::vec3 normal;  // Normal (3 floats)
    glm::vec2 uv;      // UV coordinates (2 floats)
    float ao;          // Ambient occlusion (1 float)
};

// Mesh chunk container
struct MeshChunk {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t indexCount = 0;
};

// Shader class - uses embedded source strings
class Shader {
public:
    unsigned int ID = 0;
    
    // Create shader from source strings (no file I/O)
    Shader(const char* vertexSource, const char* fragmentSource);
    ~Shader();
    
    void use() const { glUseProgram(ID); }
    
    // Uniform setters
    void setMat4(const std::string& name, const glm::mat4& mat) {
        glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
    }
    
    void setVec3(const std::string& name, float v0, float v1, float v2) {
        glUniform3f(glGetUniformLocation(ID, name.c_str()), v0, v1, v2);
    }
    
    void setVec3(const std::string& name, const glm::vec3& vec) {
        glUniform3f(glGetUniformLocation(ID, name.c_str()), vec.x, vec.y, vec.z);
    }
    
    void setFloat(const std::string& name, float value) {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
    }
    
    void setInt(const std::string& name, int value) {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
    }

private:
    // Compile a single shader
    unsigned int compileShader(unsigned int type, const std::string& source);
};

// OpenGL 3.3 renderer for chunk rendering
class OpenGLRenderer {
public:
    OpenGLRenderer();
    ~OpenGLRenderer();
    
    // Initialize OpenGL resources
    bool init();
    
    // Upload a chunk's mesh to GPU
    void uploadChunk(uint64_t id, const MeshChunk& mesh);
    
    // Destroy a chunk's GPU resources
    void destroyChunk(uint64_t id);
    
    // Frustum culling: test chunk AABB against view frustum
    // Returns true if the chunk is visible
    bool isChunkVisible(uint64_t chunkKey, const Camera& cam);
    
    // Render visible chunks
    void render(const Camera& cam, const std::vector<uint64_t>& visibleChunks);
    
    // Set the texture atlas
    void setTextureAtlas(GLuint atlas);
    
    // Set light position
    void setLightPosition(const glm::vec3& pos);
    
    // Get render distance
    int getRenderDistance() const { return renderDistance_; }
    
    // Update adaptive render distance
    void updateRenderDistance(float frameTime);
    
    // Generate a texture atlas from block types
    GLuint generateTextureAtlas();
    
private:
    // Shader program (embedded source)
    Shader shader_;
    
    // VAO/VBO/IBO for chunk meshes
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ibo_ = 0;
    
    // Texture atlas location
    GLuint textureAtlas_ = 0;
    
    // Light position
    glm::vec3 lightPos_;
    
    // Render distance (adaptive: 8-64)
    int renderDistance_;
    
    // Track uploaded chunks
    std::map<uint64_t, MeshChunk> uploadedChunks_;
    
    // Initialize VAO/VBO
    void initMeshBuffers();
    
    // Get face properties for meshing
    void getFaceProperties(int dir, float normal[3], float uvAxis1[3], float uvAxis2[3], int& faceWidthDir, int& faceHeightDir);
};
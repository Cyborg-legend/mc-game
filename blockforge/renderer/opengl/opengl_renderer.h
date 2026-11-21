#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <cstdint>
#include <array>

// Include GLEW or GLAD - we'll use GLEW for simplicity
#include <GL/glew.h>

// Forward declaration
struct Chunk;
struct MeshChunk;

// Camera for rendering
struct Camera {
    glm::vec3 position;
    float yaw;    // Y rotation in degrees
    float pitch;  // X rotation in degrees
    float fov;    // Field of view in degrees
    glm::mat4 projection; // Projection matrix

    Camera() : position(0.0f, 0.0f, 0.0f), yaw(-90.0f), pitch(0.0f), fov(70.0f) {
        updateProjection(70.0f, 4.0f / 3.0f, 0.1f, 256.0f);
    }

    void updateProjection(float fov, float aspect, float near, float far) {
        this->fov = fov;
        projection = glm::perspective(glm::radians(fov), aspect, near, far);
        // OpenGL NDC has Y pointing up, no need to flip like Vulkan
    }

    // Compute view matrix
    glm::mat4 getViewMatrix() const {
        return glm::lookAt(
            position,
            position + glm::vec3(
                cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
                sin(glm::radians(pitch)),
                sin(glm::radians(yaw)) * cos(glm::radians(pitch))
            ),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
    }
};

// Chunk renderer using OpenGL 3.3
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

private:
    // Shader program
    Shader shader_;

    // VAO/VBO for chunk meshes
    GLuint vao_;
    GLuint vbo_;
    GLuint ibo_;

    // Texture atlas location
    GLuint textureAtlas_;

    // Light position
    glm::vec3 lightPos_;

    // Render distance (adaptive: 8-64)
    int renderDistance_;

    // Track uploaded chunks
    std::map<uint64_t, MeshChunk> uploadedChunks_;

    // Initialize VAO/VBO
    void initMeshBuffers();

    // Extract face normal and UV basis from direction
    void getFaceProperties(int dir, float normal[3], float uvAxis1[3], float uvAxis2[3], int& faceWidthDir, int& faceHeightDir);
};
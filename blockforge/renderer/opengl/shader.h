#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <unordered_map>
#include "renderer/meshing/mesh.h"
#include "core/world/chunk.h"

class OpenGLRenderer {
public:
    bool Init(GLFWwindow* window);
    void Shutdown();
    void Resize(int width, int height);

    void UploadChunk(uint64_t id, const MeshChunk& mesh);
    void DestroyChunk(uint64_t id);
    void Render(const World& world, float camX, float camY, float camZ, float yaw, float pitch, float fov);

    void SetTime(float t);
    void SetRenderDistance(int dist);
    int GetRenderDistance() const { return renderDistance_; }

private:
    struct ChunkMesh {
        GLuint vao, vbo, ebo;
        int indexCount;
    };

    GLuint shader_;
    std::unordered_map<uint64_t, ChunkMesh> chunkMeshes_;
    GLuint texture_;
    int renderDistance_ = 16;
    float time_ = 0.0f;
    int width_, height_;

    GLuint CreateShader(const char* vs, const char* fs);
    void GenerateTexture();
};   
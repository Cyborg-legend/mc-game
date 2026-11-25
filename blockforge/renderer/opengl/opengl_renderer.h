#pragma once

#include <glm/glm.hpp>
#include <GL/glew.h>

#include <cstdint>
#include <map>
#include <vector>

#include "meshing/mesh.h"

struct Camera {
    glm::vec3 position{ 8.0f, 90.0f, 8.0f };
    float yaw = -1.5707963f;   // radians
    float pitch = -0.35f;      // radians
    float fovDegrees = 70.0f;
    float aspect = 16.0f / 9.0f;
    float nearPlane = 0.1f;
    float farPlane = 600.0f;

    glm::vec3 Front() const;
    void Look(float dxPixels, float dyPixels, float sensitivity = 0.0025f);
    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix() const;
};

// Per-dimension sky, fog and lighting.
struct Environment {
    glm::vec3 sky{ 0.52f, 0.72f, 0.95f };
    glm::vec3 fogColor{ 0.62f, 0.78f, 0.96f };
    glm::vec3 ambient{ 0.42f, 0.42f, 0.42f };
    glm::vec3 lightColor{ 1.0f, 0.98f, 0.92f };
    glm::vec3 lightPosition{ 96.0f, 160.0f, 96.0f };
    float fogStart = 70.0f;
    float fogDensity = 0.0022f;
};

struct HudState {
    int selectedSlot = 0;
    uint8_t hotbar[9] = { 0 };
    int health = 20;
    int maxHealth = 20;
};

class OpenGLRenderer {
public:
    OpenGLRenderer() = default;
    ~OpenGLRenderer();

    bool init();
    void shutdown();

    bool uploadChunk(uint64_t id, const MeshChunk& mesh);
    void destroyChunk(uint64_t id);
    void clearChunks();

    void render(const Camera& camera, const std::vector<uint64_t>& visibleChunks);
    void renderHud(const HudState& hud);

    // Draws an overlay batch built by UiDrawList (x, y, r, g, b, a per vertex).
    void renderUiVertices(const std::vector<float>& vertices);

    // Current drawable size in pixels; the window can be resized at runtime.
    void viewportSize(int& width, int& height) const;

    GLuint generateTextureAtlas();
    void setLightPosition(const glm::vec3& pos);
    void setEnvironment(const Environment& environment);

    static Environment UpperworldEnvironment();
    static Environment UnderworldEnvironment();

    int getDrawCalls() const { return drawCalls_; }

private:
    GLuint program_ = 0;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ibo_ = 0;
    GLuint atlas_ = 0;

    GLuint uiProgram_ = 0;
    GLuint uiVao_ = 0;
    GLuint uiVbo_ = 0;

    Environment environment_{};
    int drawCalls_ = 0;

    std::map<uint64_t, MeshChunk> uploadedChunks_;

    bool compileShaders();
    bool compileUiShaders();
    void renderSkyGradient();
    void initMeshBuffers();
    void initUiBuffers();
    void drawMesh(const MeshChunk& mesh);
};

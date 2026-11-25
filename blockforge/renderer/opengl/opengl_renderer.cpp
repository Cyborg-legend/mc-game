#include "opengl/opengl_renderer.h"

#include "core/log.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <vector>

namespace {

const char* kVertexShader = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in float aAO;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 vNormal;
out vec2 vUV;
out float vAO;
out vec3 vWorldPos;

void main() {
    vec4 world = model * vec4(aPos, 1.0);
    vWorldPos = world.xyz;
    vNormal = mat3(model) * aNormal;
    vUV = aUV;
    vAO = aAO;
    gl_Position = projection * view * world;
}
)GLSL";

const char* kFragmentShader = R"GLSL(
#version 330 core
in vec3 vNormal;
in vec2 vUV;
in float vAO;
in vec3 vWorldPos;

out vec4 FragColor;

uniform sampler2D texAtlas;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 ambientColor;
uniform vec3 lightColor;
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogDensity;

void main() {
    vec4 tex = texture(texAtlas, vUV);
    vec3 base = tex.rgb;

    vec3 n = normalize(vNormal);
    vec3 l = normalize(lightPos - vWorldPos);
    float diffuse = max(dot(n, l), 0.0);

    vec3 color = base * (ambientColor + lightColor * diffuse * 0.72) * vAO;

    // Distance fog: thin in the Upperworld, thick and red in the Underworld.
    float distanceToCamera = length(vWorldPos - viewPos);
    float fog = clamp((distanceToCamera - fogStart) * fogDensity, 0.0, 1.0);
    color = mix(color, fogColor, fog);

    FragColor = vec4(color, 1.0);
}
)GLSL";

const char* kUiVertexShader = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;

uniform vec2 screenSize;

out vec4 vColor;

void main() {
    vec2 ndc = vec2(aPos.x / screenSize.x * 2.0 - 1.0,
                    1.0 - aPos.y / screenSize.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    vColor = aColor;
}
)GLSL";

const char* kUiFragmentShader = R"GLSL(
#version 330 core
in vec4 vColor;
out vec4 FragColor;

void main() {
    FragColor = vColor;
}
)GLSL";

GLuint CompileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024] = { 0 };
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        Log::Write(std::string("Shader compile error: ") + log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint LinkProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024] = { 0 };
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        Log::Write(std::string("Program link error: ") + log);
        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// ---- Procedural tile painting -------------------------------------------

uint32_t PixelHash(int tile, int px, int py) {
    uint32_t h = static_cast<uint32_t>(tile) * 374761393u +
                 static_cast<uint32_t>(px) * 668265263u +
                 static_cast<uint32_t>(py) * 2246822519u;
    h ^= h >> 13;
    h *= 1274126177u;
    h ^= h >> 16;
    return h;
}

float PixelNoise(int tile, int px, int py) {
    return static_cast<float>(PixelHash(tile, px, py) & 0xFFFFu) / 65535.0f;
}

unsigned char Clamp255(float v) {
    if (v < 0.0f) return 0;
    if (v > 255.0f) return 255;
    return static_cast<unsigned char>(v);
}

void PaintTilePixel(const AtlasTileDef& def, int tile, int px, int py, unsigned char out[3]) {
    const float base[3] = { static_cast<float>(def.r), static_cast<float>(def.g), static_cast<float>(def.b) };
    const float accent[3] = { static_cast<float>(def.ar), static_cast<float>(def.ag), static_cast<float>(def.ab) };
    const float n = PixelNoise(tile, px, py);
    const float cx = static_cast<float>(px) - 7.5f;
    const float cy = static_cast<float>(py) - 7.5f;
    const float radius = std::sqrt(cx * cx + cy * cy);

    float shade = 1.0f;
    float accentMix = 0.0f;

    switch (def.pattern) {
        case TilePattern::Flat:
            shade = 0.92f + n * 0.12f;
            break;
        case TilePattern::Speckle:
            shade = 0.84f + n * 0.30f;
            if (n > 0.93f) shade *= 0.78f;
            break;
        case TilePattern::GrassTop:
            shade = (n > 0.70f) ? 0.78f : (0.94f + n * 0.14f);
            break;
        case TilePattern::GrassSide:
            if (py < 4) {
                accentMix = 1.0f;
                shade = 0.92f + n * 0.16f;
            } else {
                shade = 0.84f + n * 0.28f;
            }
            break;
        case TilePattern::Bark:
            shade = 0.80f + 0.22f * std::sin(static_cast<float>(px) * 1.9f + static_cast<float>(tile));
            if ((px + py * 3) % 7 == 0) shade *= 0.72f;
            shade += n * 0.08f;
            break;
        case TilePattern::Rings:
            shade = 0.76f + 0.26f * std::sin(radius * 2.1f);
            break;
        case TilePattern::Foliage:
            shade = 0.72f + n * 0.40f;
            if (n > 0.86f) shade = 0.42f;
            break;
        case TilePattern::Liquid:
            shade = 0.86f + 0.18f * std::sin(static_cast<float>(px) * 0.8f +
                                            static_cast<float>(py) * 0.5f + n * 1.5f);
            break;
        case TilePattern::Chunky: {
            const int bx = px / 4;
            const int by = py / 4;
            shade = 0.80f + 0.26f * PixelNoise(tile + bx * 7, by, 0);
            if (px % 4 == 0 || py % 4 == 0) shade *= 0.82f;
            break;
        }
        case TilePattern::Planks:
            shade = 0.92f + n * 0.12f;
            if (py % 5 == 0 || py == 15) shade = 0.62f;
            if ((px + (py / 5) * 5) % 8 == 0) shade *= 0.76f;
            break;
        case TilePattern::Grid:
            shade = (px % 6 < 1 || py % 6 < 1) ? 0.55f : (0.90f + n * 0.14f);
            break;
        case TilePattern::Brick: {
            const int row = py / 5;
            const int offset = (row % 2) * 4;
            if (py % 5 == 0 || ((px + offset) % 8) == 0) {
                accentMix = 1.0f;
                shade = 0.94f;
            } else {
                shade = 0.86f + n * 0.20f;
            }
            break;
        }
        case TilePattern::Ore:
            shade = 0.82f + n * 0.26f;
            if (radius < 3.4f + n * 2.0f) {
                accentMix = 1.0f;
                shade = 0.95f + n * 0.18f;
            }
            break;
        case TilePattern::Glow:
            shade = 1.18f - radius * 0.045f + n * 0.10f;
            if (radius < 2.6f) accentMix = 0.7f;
            break;
        case TilePattern::Swirl: {
            const float wave = std::sin((static_cast<float>(px) + static_cast<float>(py)) * 0.9f + n * 2.2f);
            accentMix = (wave > 0.15f) ? 1.0f : 0.0f;
            shade = 0.92f + n * 0.12f;
            break;
        }
        case TilePattern::Crystal: {
            const int band = (px + py * 2) % 11;
            if (band < 3) {
                accentMix = 0.55f;
                shade = 1.22f;
            } else if (band < 6) {
                shade = 0.74f;
            } else {
                shade = 0.96f + n * 0.08f;
            }
            break;
        }
        case TilePattern::Ash:
            shade = 0.90f + n * 0.16f;
            if (n > 0.93f) shade = 0.62f;
            break;
        case TilePattern::Unused:
            shade = 0.0f;
            break;
    }

    for (int i = 0; i < 3; ++i) {
        const float value = base[i] * shade + (accent[i] * shade - base[i] * shade) * accentMix;
        out[i] = Clamp255(value);
    }
}

// A UI vertex is position (2 floats) plus colour (4 floats).
void PushQuad(std::vector<float>& out,
              float x, float y, float w, float h,
              float r, float g, float b, float a) {
    const float x0 = x;
    const float y0 = y;
    const float x1 = x + w;
    const float y1 = y + h;
    const float verts[6][6] = {
        { x0, y0, r, g, b, a },
        { x1, y0, r, g, b, a },
        { x1, y1, r, g, b, a },
        { x0, y0, r, g, b, a },
        { x1, y1, r, g, b, a },
        { x0, y1, r, g, b, a },
    };
    for (const auto& v : verts) {
        out.insert(out.end(), v, v + 6);
    }
}

} // namespace

glm::vec3 Camera::Front() const {
    return glm::normalize(glm::vec3(
        std::cos(pitch) * std::cos(yaw),
        std::sin(pitch),
        std::cos(pitch) * std::sin(yaw)));
}

void Camera::Look(float dxPixels, float dyPixels, float sensitivity) {
    yaw += dxPixels * sensitivity;
    pitch -= dyPixels * sensitivity;
    const float limit = 1.55f;
    if (pitch > limit) pitch = limit;
    if (pitch < -limit) pitch = -limit;
}

glm::mat4 Camera::GetViewMatrix() const {
    return glm::lookAt(position, position + Front(), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::GetProjectionMatrix() const {
    return glm::perspective(glm::radians(fovDegrees), aspect, nearPlane, farPlane);
}

Environment OpenGLRenderer::UpperworldEnvironment() {
    Environment env;
    env.sky = glm::vec3(0.52f, 0.72f, 0.95f);
    env.fogColor = glm::vec3(0.62f, 0.78f, 0.96f);
    env.ambient = glm::vec3(0.44f, 0.44f, 0.46f);
    env.lightColor = glm::vec3(1.0f, 0.98f, 0.90f);
    env.lightPosition = glm::vec3(96.0f, 160.0f, 96.0f);
    env.fogStart = 80.0f;
    env.fogDensity = 0.0018f;
    return env;
}

Environment OpenGLRenderer::UnderworldEnvironment() {
    Environment env;
    env.sky = glm::vec3(0.10f, 0.03f, 0.03f);
    env.fogColor = glm::vec3(0.20f, 0.05f, 0.04f);
    env.ambient = glm::vec3(0.34f, 0.16f, 0.14f);
    env.lightColor = glm::vec3(1.0f, 0.52f, 0.22f);
    env.lightPosition = glm::vec3(0.0f, 24.0f, 0.0f);
    env.fogStart = 22.0f;
    env.fogDensity = 0.0075f;
    return env;
}

OpenGLRenderer::~OpenGLRenderer() {
    shutdown();
}

bool OpenGLRenderer::compileShaders() {
    program_ = LinkProgram(kVertexShader, kFragmentShader);
    return program_ != 0;
}

bool OpenGLRenderer::compileUiShaders() {
    uiProgram_ = LinkProgram(kUiVertexShader, kUiFragmentShader);
    return uiProgram_ != 0;
}

void OpenGLRenderer::initMeshBuffers() {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ibo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, pos)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, uv)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, ao)));

    glBindVertexArray(0);
}

void OpenGLRenderer::initUiBuffers() {
    glGenVertexArrays(1, &uiVao_);
    glGenBuffers(1, &uiVbo_);

    glBindVertexArray(uiVao_);
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));
    glBindVertexArray(0);
}

bool OpenGLRenderer::init() {
    // A 3.3 core profile exposes no legacy entry points, so GLEW has to probe
    // extensions experimentally; without this glewInit() fails on many drivers
    // (Intel included) and the window closes right after it opens.
    glewExperimental = GL_TRUE;
    const GLenum glewStatus = glewInit();
    if (glewStatus != GLEW_OK) {
        Log::Write(std::string("GLEW init failed: ") +
                   reinterpret_cast<const char*>(glewGetErrorString(glewStatus)));
        return false;
    }
    // The experimental probe can leave one GL_INVALID_ENUM behind; clear it so
    // the game's own error checks start clean.
    while (glGetError() != GL_NO_ERROR) {
    }

    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* rendererName = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    Log::Write(std::string("OpenGL ") + (version ? version : "unknown") +
               " | " + (rendererName ? rendererName : "unknown renderer"));

    if (!GLEW_VERSION_3_3) {
        Log::Write("OpenGL 3.3 is required but the driver does not expose it.");
        return false;
    }

    glfwSwapInterval(1);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    if (!compileShaders()) {
        Log::Write("World shader failed to build.");
        return false;
    }
    if (!compileUiShaders()) {
        Log::Write("UI shader failed to build.");
        return false;
    }

    initMeshBuffers();
    initUiBuffers();
    generateTextureAtlas();

    environment_ = UpperworldEnvironment();
    return true;
}

void OpenGLRenderer::shutdown() {
    if (vbo_) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (ibo_) { glDeleteBuffers(1, &ibo_); ibo_ = 0; }
    if (vao_) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    if (uiVbo_) { glDeleteBuffers(1, &uiVbo_); uiVbo_ = 0; }
    if (uiVao_) { glDeleteVertexArrays(1, &uiVao_); uiVao_ = 0; }
    if (atlas_) { glDeleteTextures(1, &atlas_); atlas_ = 0; }
    if (program_) { glDeleteProgram(program_); program_ = 0; }
    if (uiProgram_) { glDeleteProgram(uiProgram_); uiProgram_ = 0; }
    uploadedChunks_.clear();
}

bool OpenGLRenderer::uploadChunk(uint64_t id, const MeshChunk& mesh) {
    uploadedChunks_[id] = mesh;
    return true;
}

void OpenGLRenderer::destroyChunk(uint64_t id) {
    uploadedChunks_.erase(id);
}

void OpenGLRenderer::clearChunks() {
    uploadedChunks_.clear();
}

void OpenGLRenderer::setEnvironment(const Environment& environment) {
    environment_ = environment;
}

void OpenGLRenderer::drawMesh(const MeshChunk& mesh) {
    if (mesh.vertices.empty() || mesh.indices.empty()) {
        return;
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(Vertex)),
        mesh.vertices.data(),
        GL_STREAM_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(uint32_t)),
        mesh.indices.data(),
        GL_STREAM_DRAW);

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indices.size()), GL_UNSIGNED_INT, nullptr);
    ++drawCalls_;
}

void OpenGLRenderer::render(const Camera& camera, const std::vector<uint64_t>& visibleChunks) {
    drawCalls_ = 0;

    glClearColor(environment_.sky.r, environment_.sky.g, environment_.sky.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!program_) {
        return;
    }

    glUseProgram(program_);

    const glm::mat4 model(1.0f);
    const glm::mat4 view = camera.GetViewMatrix();
    const glm::mat4 projection = camera.GetProjectionMatrix();

    glUniformMatrix4fv(glGetUniformLocation(program_, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(program_, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(program_, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(glGetUniformLocation(program_, "lightPos"), 1, glm::value_ptr(environment_.lightPosition));
    glUniform3fv(glGetUniformLocation(program_, "viewPos"), 1, glm::value_ptr(camera.position));
    glUniform3fv(glGetUniformLocation(program_, "ambientColor"), 1, glm::value_ptr(environment_.ambient));
    glUniform3fv(glGetUniformLocation(program_, "lightColor"), 1, glm::value_ptr(environment_.lightColor));
    glUniform3fv(glGetUniformLocation(program_, "fogColor"), 1, glm::value_ptr(environment_.fogColor));
    glUniform1f(glGetUniformLocation(program_, "fogStart"), environment_.fogStart);
    glUniform1f(glGetUniformLocation(program_, "fogDensity"), environment_.fogDensity);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas_);
    glUniform1i(glGetUniformLocation(program_, "texAtlas"), 0);

    glBindVertexArray(vao_);
    for (uint64_t key : visibleChunks) {
        auto it = uploadedChunks_.find(key);
        if (it != uploadedChunks_.end()) {
            drawMesh(it->second);
        }
    }
    glBindVertexArray(0);
}

void OpenGLRenderer::viewportSize(int& width, int& height) const {
    GLint viewport[4] = { 0, 0, 1280, 720 };
    glGetIntegerv(GL_VIEWPORT, viewport);
    width = viewport[2] > 0 ? viewport[2] : 1280;
    height = viewport[3] > 0 ? viewport[3] : 720;
}

void OpenGLRenderer::renderUiVertices(const std::vector<float>& vertices) {
    if (!uiProgram_ || vertices.empty()) {
        return;
    }

    int viewportW = 1280;
    int viewportH = 720;
    viewportSize(viewportW, viewportH);
    const float width = static_cast<float>(viewportW);
    const float height = static_cast<float>(viewportH);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(uiProgram_);
    glUniform2f(glGetUniformLocation(uiProgram_, "screenSize"), width, height);

    glBindVertexArray(uiVao_);
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                 vertices.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 6));
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

void OpenGLRenderer::renderHud(const HudState& hud) {
    if (!uiProgram_) {
        return;
    }

    int viewportW = 1280;
    int viewportH = 720;
    viewportSize(viewportW, viewportH);
    const float width = static_cast<float>(viewportW);
    const float height = static_cast<float>(viewportH);

    std::vector<float> vertices;
    vertices.reserve(6 * 6 * 48);

    // Crosshair.
    const float cx = width * 0.5f;
    const float cy = height * 0.5f;
    PushQuad(vertices, cx - 10.0f, cy - 1.0f, 20.0f, 2.0f, 1.0f, 1.0f, 1.0f, 0.85f);
    PushQuad(vertices, cx - 1.0f, cy - 10.0f, 2.0f, 20.0f, 1.0f, 1.0f, 1.0f, 0.85f);

    // Hotbar.
    constexpr int kSlots = 9;
    const float slot = 44.0f;
    const float gap = 4.0f;
    const float totalWidth = kSlots * slot + (kSlots - 1) * gap;
    const float barX = (width - totalWidth) * 0.5f;
    const float barY = height - slot - 18.0f;

    for (int i = 0; i < kSlots; ++i) {
        const float x = barX + static_cast<float>(i) * (slot + gap);
        const bool selected = (i == hud.selectedSlot);

        if (selected) {
            PushQuad(vertices, x - 3.0f, barY - 3.0f, slot + 6.0f, slot + 6.0f, 1.0f, 1.0f, 1.0f, 0.95f);
        } else {
            PushQuad(vertices, x - 3.0f, barY - 3.0f, slot + 6.0f, slot + 6.0f, 0.20f, 0.20f, 0.24f, 0.80f);
        }
        PushQuad(vertices, x, barY, slot, slot, 0.04f, 0.04f, 0.05f, 0.85f);

        float color[3] = { 0.1f, 0.1f, 0.1f };
        BlockColor(static_cast<BlockType>(hud.hotbar[i]), color);
        PushQuad(vertices, x + 5.0f, barY + 5.0f, slot - 10.0f, slot - 10.0f,
                 color[0], color[1], color[2], 0.95f);
    }

    // Health bar.
    const int hearts = 10;
    const int maxHealth = (hud.maxHealth > 0) ? hud.maxHealth : 20;
    for (int i = 0; i < hearts; ++i) {
        const bool full = hud.health > i * (maxHealth / hearts);
        const float r = full ? 0.86f : 0.28f;
        const float g = full ? 0.16f : 0.12f;
        const float b = full ? 0.20f : 0.14f;
        PushQuad(vertices, 20.0f + static_cast<float>(i) * 22.0f, height - 44.0f,
                 16.0f, 16.0f, r, g, b, 0.95f);
    }

    renderUiVertices(vertices);
}

GLuint OpenGLRenderer::generateTextureAtlas() {
    constexpr int kAtlasSize = 256;
    constexpr int kTile = 16;
    static unsigned char pixels[kAtlasSize * kAtlasSize * 4];

    for (int i = 0; i < kAtlasSize * kAtlasSize; ++i) {
        pixels[i * 4 + 0] = 0;
        pixels[i * 4 + 1] = 0;
        pixels[i * 4 + 2] = 0;
        pixels[i * 4 + 3] = 255;
    }

    const int tileCount = AtlasTileCount();
    for (int tile = 0; tile < tileCount; ++tile) {
        const AtlasTileDef& def = GetAtlasTile(tile);
        if (def.pattern == TilePattern::Unused) {
            continue;
        }

        const int originX = (tile % 16) * kTile;
        const int originY = (tile / 16) * kTile;

        for (int py = 0; py < kTile; ++py) {
            for (int px = 0; px < kTile; ++px) {
                unsigned char rgb[3] = { 0, 0, 0 };
                PaintTilePixel(def, tile, px, py, rgb);
                unsigned char* p = &pixels[((originY + py) * kAtlasSize + (originX + px)) * 4];
                p[0] = rgb[0];
                p[1] = rgb[1];
                p[2] = rgb[2];
                p[3] = 255;
            }
        }
    }

    if (!atlas_) {
        glGenTextures(1, &atlas_);
    }
    glBindTexture(GL_TEXTURE_2D, atlas_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kAtlasSize, kAtlasSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return atlas_;
}

void OpenGLRenderer::setLightPosition(const glm::vec3& pos) {
    environment_.lightPosition = pos;
}

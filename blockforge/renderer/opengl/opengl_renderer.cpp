#include "opengl_renderer.h"
#include <cmath>

static const char* VS_SRC = "#version 330 core\n"
"layout(location=0) in vec3 aPos;\n"
"layout(location=1) in vec3 aNormal;\n"
"layout(location=2) in vec2 aUV;\n"
"layout(location=3) in float aAO;\n"
"uniform mat4 model, view, projection;\n"
"out vec3 vNormal;\n"
"out vec2 vUV;\n"
"out float vAO;\n"
"void main(){\n"
"  gl_Position = projection * view * model * vec4(aPos,1.0);\n"
"  vNormal = aNormal;\n"
"  vUV = aUV;\n"
"  vAO = aAO;\n"
"}\n";

static const char* FS_SRC = "#version 330 core\n"
"in vec3 vNormal;\n"
"in vec2 vUV;\n"
"in float vAO;\n"
"uniform sampler2D tex;\n"
"uniform vec3 sunDir;\n"
"uniform float time;\n"
"out vec4 fragColor;\n"
"void main(){\n"
"  vec3 col = texture(tex, vUV).rgb;\n"
"  float diff = max(dot(normalize(vNormal), sunDir), 0.0);\n"
"  float ambient = 0.4;\n"
"  vec3 light = (ambient + diff * 0.6) * col * vAO;\n"
"  float dayFactor = 0.5 + 0.5 * sin(time * 0.05);\n"
"  light *= (0.3 + 0.7 * dayFactor);\n"
"  fragColor = vec4(light, 1.0);\n"
"}\n";

bool OpenGLRenderer::Init(GLFWwindow* window) {
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) return false;

    shader_ = CreateShader(VS_SRC, FS_SRC);
    if (shader_ == 0) return false;

    GenerateTexture();

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.4f, 0.6f, 0.9f, 1.0f);

    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    width_ = w;
    height_ = h;
    return true;
}

void OpenGLRenderer::Shutdown() {
    for (auto& pair : chunkMeshes_) {
        glDeleteVertexArrays(1, &pair.second.vao);
        glDeleteBuffers(1, &pair.second.vbo);
        glDeleteBuffers(1, &pair.second.ebo);
    }
    chunkMeshes_.clear();
    if (shader_) glDeleteProgram(shader_);
    if (texture_) glDeleteTextures(1, &texture_);
}

void OpenGLRenderer::Resize(int w, int h) {
    width_ = w;
    height_ = h;
    glViewport(0, 0, w, h);
}

GLuint OpenGLRenderer::CreateShader(const char* vs, const char* fs) {
    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v, 1, &vs, nullptr);
    glCompileShader(v);

    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f, 1, &fs, nullptr);
    glCompileShader(f);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, v);
    glAttachShader(prog, f);
    glLinkProgram(prog);
    glDeleteShader(v);
    glDeleteShader(f);
    return prog;
}

void OpenGLRenderer::GenerateTexture() {
    // 16x16 pixel art atlas, 4x4 grid of 16x16 tiles = 64x64
    unsigned char pixels[64 * 64 * 4];
    memset(pixels, 0, sizeof(pixels));

    auto setPixel = [&](int x, int y, unsigned char r, unsigned char g, unsigned char b) {
        int idx = (y * 64 + x) * 4;
        pixels[idx] = r;
        pixels[idx+1] = g;
        pixels[idx+2] = b;
        pixels[idx+3] = 255;
    };

    // Tile 0: Grass top (green)
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++)
            setPixel(x, y, 80 + (x*3)%20, 160 + (y*2)%30, 60);

    // Tile 1: Dirt (brown)
    for (int y = 16; y < 32; y++)
        for (int x = 0; x < 16; x++)
            setPixel(x, y, 130 + (x*2)%15, 90 + (y*2)%10, 60);

    // Tile 2: Stone (gray)
    for (int y = 32; y < 48; y++)
        for (int x = 16; x < 32; x++)
            setPixel(x, y, 120 + (x+y)%20, 120 + (x+y)%20, 120 + (x+y)%20);

    // Tile 3: Wood (brown vertical stripes)
    for (int y = 48; y < 64; y++)
        for (int x = 32; x < 48; x++)
            setPixel(x, y, 100 + (x%4)*10, 70, 40);

    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void OpenGLRenderer::UploadChunk(uint64_t id, const MeshChunk& mesh) {
    if (mesh.vertexCount == 0) return;

    ChunkMesh cm;
    glGenVertexArrays(1, &cm.vao);
    glGenBuffers(1, &cm.vbo);
    glGenBuffers(1, &cm.ebo);
    cm.indexCount = (int)mesh.indexCount;

    glBindVertexArray(cm.vao);
    glBindBuffer(GL_ARRAY_BUFFER, cm.vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(Vertex), mesh.vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cm.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(uint32_t), mesh.indices.data(), GL_STATIC_DRAW);

    // Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    // Normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    // UV
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    // AO
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, ao));

    glBindVertexArray(0);
    chunkMeshes_[id] = cm;
}

void OpenGLRenderer::DestroyChunk(uint64_t id) {
    auto it = chunkMeshes_.find(id);
    if (it != chunkMeshes_.end()) {
        glDeleteVertexArrays(1, &it->second.vao);
        glDeleteBuffers(1, &it->second.vbo);
        glDeleteBuffers(1, &it->second.ebo);
        chunkMeshes_.erase(it);
    }
}

void OpenGLRenderer::SetTime(float t) { time_ = t; }
void OpenGLRenderer::SetRenderDistance(int d) { renderDistance_ = d; }

void OpenGLRenderer::Render(const World& world, float camX, float camY, float camZ, float yaw, float pitch, float fov) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 projection = glm::perspective(glm::radians(fov), (float)width_ / (float)height_, 0.1f, 1000.0f);
    glm::mat4 view = glm::lookAt(
        glm::vec3(camX, camY - 0.5f, camZ),
        glm::vec3(camX - sinf(yaw) * cosf(pitch), camY - 0.5f + sinf(pitch), camZ - cosf(yaw) * cosf(pitch)),
        glm::vec3(0, 1, 0)
    );

    glUseProgram(shader_);
    glUniformMatrix4fv(glGetUniformLocation(shader_, "projection"), 1, GL_FALSE, &projection[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader_, "view"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader_, "model"), 1, GL_FALSE, &glm::mat4(1.0f)[0][0]);
    glUniform1f(glGetUniformLocation(shader_, "time"), time_);
    glUniform3f(glGetUniformLocation(shader_, "sunDir"), 0.5f, 1.0f, 0.3f);
    glUniform1i(glGetUniformLocation(shader_, "tex"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_);

    for (auto& pair : chunkMeshes_) {
        int cx, cy, cz;
        World::UnpackCoords(pair.first, cx, cy, cz);

        // Simple distance cull
        float dx = (float)(cx * CHUNK_SIZE + 8 - (int)camX);
        float dz = (float)(cz * CHUNK_SIZE + 8 - (int)camZ);
        float dist = sqrtf(dx*dx + dz*dz);
        if (dist > renderDistance_ * CHUNK_SIZE) continue;

        // Frustum cull (simple)
        float chunkCenterX = (float)(cx * CHUNK_SIZE + 8);
        float chunkCenterY = (float)(cy * CHUNK_SIZE + 8);
        float chunkCenterZ = (float)(cz * CHUNK_SIZE + 8);
        glm::vec4 clip = projection * view * glm::vec4(chunkCenterX, chunkCenterY, chunkCenterZ, 1.0f);
        if (std::abs(clip.x) > clip.w || std::abs(clip.y) > clip.w || std::abs(clip.z) > clip.w)
            continue;

        glBindVertexArray(pair.second.vao);
        glDrawElements(GL_TRIANGLES, pair.second.indexCount, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);
}   
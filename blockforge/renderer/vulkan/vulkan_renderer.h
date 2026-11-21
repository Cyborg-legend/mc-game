#pragma once

#include "vulkan_context.h"
#include "core/world/chunk.h"
#include "renderer/meshing/greedy_mesher.h"
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

// Camera for rendering
struct Camera {
    glm::vec3 position;
    float yaw;    // Y rotation in degrees
    float pitch;  // X rotation in degrees
    float fov;    // Field of view in degrees
    glm::mat4 projection; // Projection matrix

    Camera() : position(0.0f, 0.0f, 0.0f), yaw(0.0f), pitch(0.0f), fov(70.0f) {
        UpdateProjection(70.0f, 4.0f / 3.0f, 0.1f, 256.0f);
    }

    void UpdateProjection(float fov, float aspect, float near, float far) {
        this->fov = fov;
        projection = glm::perspective(glm::radians(fov), aspect, near, far);
        // Vulkan NDC has Y pointing down, so flip Y
        projection[1][1] *= -1;
    }
};

// Frame state for rendering
struct FrameState {
    VkCommandBuffer commandBuffer;
    VkFramebuffer framebuffer;
    VkImageView imageView;
};

// ChunkRenderer: handles uploading and rendering chunks
class ChunkRenderer {
public:
    ChunkRenderer(VulkanContext& ctx) : context(ctx) {}

    // Upload a chunk's mesh to GPU
    void UploadChunk(uint64_t id, const MeshChunk& mesh) {
        // In a full implementation, this would create VBO+IBO
        // For now, just track the mesh
        uploadedChunks_[id] = mesh;
    }

    // Destroy a chunk's GPU resources
    void DestroyChunk(uint64_t id) {
        uploadedChunks_.erase(id);
    }

    // Frustum culling: test chunk AABB against 6 frustum planes
    // Returns true if the chunk is visible
    bool IsChunkVisible(uint64_t chunkKey, const Camera& cam) const {
        // Unpack chunk coordinates
        int cx, cy, cz;
        World::UnpackCoords(chunkKey, cx, cy, cz);

        // Chunk AABB: 16×16×16 blocks, each block is 1 unit
        // Chunk center at (cx*16+8, cy*16+8, cz*16+8)
        // AABB extents: 8 units from center
        float cxf = static_cast<float>(cx * 16 + 8);
        float cyf = static_cast<float>(cy * 16 + 8);
        float czf = static_cast<float>(cz * 16 + 8);
        float extent = 8.0f;

        // Frustum planes: [normal, distance]
        // We need the view-projection matrix to extract planes
        // For now, do a simple AABB vs sphere test
        // Distance from camera to chunk center
        float dx = cxf - cam.position.x;
        float dy = cyf - cam.position.y;
        float dz = czf - cam.position.z;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

        // If distance > far plane or < near plane, cull
        // This is a placeholder - real frustum culling uses plane equations
        return dist < 64.0f; // Simple distance culling
    }

    // Render visible chunks
    void Render(const Camera& cam, const std::vector<uint64_t>& visibleChunks) {
        // In a full implementation, this would:
        // 1. Acquire swapchain image
        // 2. Update camera uniforms
        // 3. Record command buffer with drawing commands
        // 4. Present
        (void)cam;
        (void)visibleChunks;
    }

    // Adaptive render distance
    int renderDistance = 16; // Start at 16 chunks
    int renderDistanceTimer = 0;
    float lastFrameTime = 0.0f;

    // Update render distance based on frame time
    void UpdateRenderDistance(float frameTime) {
        renderDistanceTimer += static_cast<int>(frameTime * 1000.0f); // Convert to ms

        if (renderDistanceTimer >= 10000) { // 10 seconds
            // Check average frame time - if < 16ms, increase; if > 33ms, decrease
            if (lastFrameTime < 16.0f / 1000.0f) {
                // Increase render distance
                if (renderDistance < 64) {
                    renderDistance += 2;
                }
            } else if (lastFrameTime > 33.0f / 1000.0f) {
                // Decrease render distance
                if (renderDistance > 8) {
                    renderDistance -= 2;
                }
            }
            renderDistanceTimer = 0;
        }
        // Clamp range: 8-64
        if (renderDistance < 8) renderDistance = 8;
        if (renderDistance > 64) renderDistance = 64;
    }

private:
    VulkanContext& context;
    // Track uploaded chunks
    std::map<uint64_t, MeshChunk> uploadedChunks_;
};
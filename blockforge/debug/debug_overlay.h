#pragma once

#include <string>
#include <sstream>

struct DebugOverlay {
    bool enabled = false;
    bool shown = false;
    
    // FPS tracking
    float fps = 0.0f;
    float frameTime = 0.0f;
    
    // Rendering stats
    uint32_t drawCalls = 0;
    uint32_t chunkCount = 0;
    uint32_t entityCount = 0;
    
    // Player position
    float playerX = 0.0f;
    float playerY = 0.0f;
    float playerZ = 0.0f;
    
    // Player facing
    float playerYaw = 0.0f;
    float playerPitch = 0.0f;
    
    // Initialize
    void Initialize();
    
    // Shut down
    void Shutdown();
    
    // Toggle visibility
    void Toggle();
    
    // Update stats
    void UpdateStats(float ft, uint32_t dc, uint32_t cc, uint32_t ec,
                     float px, float py, float pz, float yaw, float pitch);
    
    // Render debug text (called from renderer)
    // In a full implementation, this would use ImGui or similar
    void Render();
};
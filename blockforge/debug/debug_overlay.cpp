#include "debug_overlay.h"
#include <chrono>

void DebugOverlay::Initialize() {
    enabled = true;
    shown = true;
    fps = 0.0f;
    frameTime = 0.0f;
    drawCalls = 0;
    chunkCount = 0;
    entityCount = 0;
    playerX = 0.0f;
    playerY = 0.0f;
    playerZ = 0.0f;
    playerYaw = 0.0f;
    playerPitch = 0.0f;
}

void DebugOverlay::Shutdown() {
}

void DebugOverlay::Toggle() {
    shown = !shown;
}

void DebugOverlay::UpdateStats(float ft, uint32_t dc, uint32_t cc, uint32_t ec,
                               float px, float py, float pz, float yaw, float pitch) {
    frameTime = ft;
    drawCalls = dc;
    chunkCount = cc;
    entityCount = ec;
    playerX = px;
    playerY = py;
    playerZ = pz;
    playerYaw = yaw;
    playerPitch = pitch;
    
    // Calculate FPS
    static auto lastTime = std::chrono::high_resolution_clock::now();
    static int frameCount = 0;
    frameCount++;
    
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
    if (duration.count() >= 1.0) {
        fps = static_cast<float>(frameCount) / static_cast<float>(duration.count());
        frameCount = 0;
        lastTime = now;
    }
}

void DebugOverlay::Render() {
    if (!shown) return;
    
    // In a full implementation, this would use ImGui or similar to render text overlay
    // For now, just output to console
    if (enabled) {
        std::stringstream ss;
        ss << "BlockForge Debug [F3]";
        ss << "\nFPS: " << (fps > 0 ? std::to_string((int)fps) : "--");
        ss << "\nFrame Time: " << (frameTime > 0 ? std::to_string(frameTime * 1000.0f) + "ms" : "--");
        ss << "\nDraw Calls: " << drawCalls;
        ss << "\nChunks: " << chunkCount;
        ss << "\nEntities: " << entityCount;
        ss << "\nPos: (" << playerX << ", " << playerY << ", " << playerZ << ")";
        ss << "\nFacing: Yaw=" << playerYaw << ", Pitch=" << playerPitch;
        // OutputDebugStringA(ss.str().c_str()); // Windows
        // __android_log_write(ANDROID_LOG_INFO, "BlockForge", ss.str().c_str()); // Android
    }
}
#include "debug/debug_overlay.h"

#include <chrono>
#include <iostream>

void DebugOverlay::Initialize() {
    shown = true;
    enabled = true;
    fps = 0.0f;
    frameTime = 0.0f;
    drawCalls = 0;
    chunkCount = 0;
    entityCount = 0;
}

void DebugOverlay::Shutdown() {
    shown = false;
}

void DebugOverlay::Toggle() {
    shown = !shown;
}

void DebugOverlay::UpdateStats(float frameTimeSeconds,
                               uint32_t dc,
                               uint32_t cc,
                               uint32_t ec,
                               float px,
                               float py,
                               float pz,
                               float yaw,
                               float pitch,
                               const char* dimension) {
    frameTime = frameTimeSeconds;
    drawCalls = dc;
    chunkCount = cc;
    entityCount = ec;
    playerX = px;
    playerY = py;
    playerZ = pz;
    playerYaw = yaw;
    playerPitch = pitch;
    dimensionName = (dimension != nullptr) ? dimension : "Unknown";

    if (frameTime > 0.0f) {
        fps = 1.0f / frameTime;
    }
}

void DebugOverlay::Render() {
    if (!shown || !enabled) {
        return;
    }
    static int frameCounter = 0;
    if ((++frameCounter % 60) != 0) {
        return;
    }
    std::cout << "[debug] fps=" << static_cast<int>(fps)
              << " frame=" << (frameTime * 1000.0f) << "ms"
              << " draws=" << drawCalls
              << " chunks=" << chunkCount
              << " entities=" << entityCount
              << " realm=" << dimensionName
              << " pos=(" << playerX << "," << playerY << "," << playerZ << ")"
              << std::endl;
}

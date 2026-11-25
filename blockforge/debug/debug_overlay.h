#pragma once

#include <cstdint>

// Lightweight text overlay state. Rendered to stdout; a graphical overlay can
// replace Render() without touching call sites.
class DebugOverlay {
public:
    static void Initialize();
    static void Shutdown();
    static void Toggle();

    static void UpdateStats(float frameTimeSeconds,
                            uint32_t drawCalls,
                            uint32_t chunkCount,
                            uint32_t entityCount,
                            float playerX,
                            float playerY,
                            float playerZ,
                            float yaw,
                            float pitch,
                            const char* dimension);

    static void Render();

    static inline bool shown = true;
    static inline bool enabled = true;
    static inline float fps = 0.0f;
    static inline float frameTime = 0.0f;
    static inline uint32_t drawCalls = 0;
    static inline uint32_t chunkCount = 0;
    static inline uint32_t entityCount = 0;
    static inline float playerX = 0.0f;
    static inline float playerY = 0.0f;
    static inline float playerZ = 0.0f;
    static inline float playerYaw = 0.0f;
    static inline float playerPitch = 0.0f;
    static inline const char* dimensionName = "Upperworld";
};

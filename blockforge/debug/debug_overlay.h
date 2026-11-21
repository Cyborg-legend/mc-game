#pragma once

#include <string>
#include <sstream>

// Debug overlay for F3 info
class DebugOverlay {
public:
    // Update FPS and frame time info
    static std::string GetFPSText(float fps, float frameTimeMs) {
        std::ostringstream oss;
        oss << "BlockForge F3" << std::endl;
        oss << "FPS: " << std::fixed << std::setprecision(1) << fps << std::endl;
        oss << "Frame time: " << std::fixed << std::setprecision(1) << frameTimeMs << "ms" << std::endl;
        return oss.str();
    }

    // Get draw calls text
    static std::string GetDrawCallText(uint32_t drawCalls) {
        std::ostringstream oss;
        oss << "Draw calls: " << drawCalls << std::endl;
        return oss.str();
    }

    // Get entity/chunk count text
    static std::string GetEntityText(uint32_t entityCount) {
        std::ostringstream oss;
        oss << "Entities: " << entityCount << std::endl;
        return oss.str();
    }

    static std::string GetChunkText(uint32_t chunkCount) {
        std::ostringstream oss;
        oss << "Chunks: " << chunkCount << std::endl;
        return oss.str();
    }

    // Get player position text
    static std::string GetPositionText(float x, float y, float z) {
        std::ostringstream oss;
        oss << "Pos: (" << std::fixed << std::setprecision(2) << x 
            << ", " << y << ", " << z << ")" << std::endl;
        return oss.str();
    }

    // Get facing direction text
    static std::string GetFacingText(float yaw, float pitch) {
        std::ostringstream oss;
        oss << "Looking: Yaw " << std::fixed << std::setprecision(1) << yaw 
            << ", Pitch " << pitch << std::endl;
        return oss.str();
    }
};
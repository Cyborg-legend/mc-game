#pragma once

#include "entity.h"

namespace core::ecs {

enum class MobType : uint8_t {
    None,
    Player,
    Sheep,
    Wolf,
    Zombie,
    // Keep last for array sizing
    Count
};

inline const char* MobTypeToString(MobType type) {
    switch (type) {
        case MobType::Player: return "player";
        case MobType::Sheep: return "sheep";
        case MobType::Wolf: return "wolf";
        case MobType::Zombie: return "zombie";
        default: return "none";
    }
}

} // namespace core::ecs
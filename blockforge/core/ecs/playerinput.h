#pragma once

#include "entity.h"

namespace core::ecs {

enum class PlayerInput : uint8_t {
    None,
    Forward,
    Back,
    Left,
    Right,
    Jump,
    Sprint,
    Use,
    Count
};

inline const char* PlayerInputToString(PlayerInput input) {
    switch (input) {
        case PlayerInput::Forward: return "forward";
        case PlayerInput::Back: return "back";
        case PlayerInput::Left: return "left";
        case PlayerInput::Right: return "right";
        case PlayerInput::Jump: return "jump";
        case PlayerInput::Sprint: return "sprint";
        case PlayerInput::Use: return "use";
        default: return "none";
    }
}

} // namespace core::ecs
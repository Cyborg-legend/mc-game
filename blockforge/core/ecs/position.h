#pragma once

#include "entity.h"
#include <array>

namespace core::ecs {

struct Position {
    float x, y, z;
    // SoA arrays will store these separately
    static constexpr int stride = 3;
};

} // namespace core::ecs
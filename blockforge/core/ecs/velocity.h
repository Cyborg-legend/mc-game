#pragma once

#include "entity.h"
#include <array>

namespace core::ecs {

struct Velocity {
    float x, y, z;
    static constexpr int stride = 3;
};

} // namespace core::ecs
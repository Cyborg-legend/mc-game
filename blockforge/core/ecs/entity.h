#pragma once

#include <cstdint>

namespace core::ecs {

using Entity = uint32_t;

// Maximum number of entities and components
static constexpr Entity MAX_ENTITIES = 65535;
static constexpr int MAX_COMPONENTS = 32;

} // namespace core::ecs
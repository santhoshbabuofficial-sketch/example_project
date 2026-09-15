#pragma once

#include <cstdint>

namespace bldc {

enum class Phase : uint8_t { kU = 0U, kV = 1U, kW = 2U };

enum class ZeroCrossEdge : uint8_t { kRising = 0U, kFalling = 1U };

}  // namespace bldc

#pragma once

#include <array>

#include "bldc_types.hpp"

namespace bldc {

struct CommutationStep {
    Phase high_phase;      // Driven with PWM (high-side).
    Phase low_phase;       // Grounded (low-side fully on).
    Phase floating_phase;  // Tri-stated; sampled for the back-EMF zero cross.
    ZeroCrossEdge expected_edge;
};

// Standard six-step trapezoidal sequence for forward rotation. To reverse
// direction, walk the table backwards instead of forwards.
inline constexpr std::array<CommutationStep, 6U> kCommutationTable{{
    {Phase::kU, Phase::kV, Phase::kW, ZeroCrossEdge::kRising},
    {Phase::kU, Phase::kW, Phase::kV, ZeroCrossEdge::kFalling},
    {Phase::kV, Phase::kW, Phase::kU, ZeroCrossEdge::kRising},
    {Phase::kV, Phase::kU, Phase::kW, ZeroCrossEdge::kFalling},
    {Phase::kW, Phase::kU, Phase::kV, ZeroCrossEdge::kRising},
    {Phase::kW, Phase::kV, Phase::kU, ZeroCrossEdge::kFalling},
}};

}  // namespace bldc

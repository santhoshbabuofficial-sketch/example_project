#pragma once

#include <array>                                          // Includes fixed-size array container.
#include <cstdint>                                        // Includes fixed-size integer types.


namespace motor_control {                              // Opens motor control task namespace.


enum class Phase : uint8_t {                              // Creates enum identifying the three inverter phases.

    kU = 0U,                                              // Phase U -> TIM1_CH1 -> PC0.

    kV = 1U,                                              // Phase V -> TIM1_CH2 -> PC1.

    kW = 2U,                                              // Phase W -> TIM1_CH3 -> PC2.
};


// One entry of the six-step trapezoidal sequence. In every step exactly
// one phase is PWM-driven, one is pulled to ground, and one floats - which
// is what produces the classic trapezoidal phase waveform on a scope.
struct CommutationStep {                                  // Creates structure describing a single commutation step.

    Phase high_phase;                                     // Phase driven with PWM (current flows in here).

    Phase low_phase;                                      // Phase held at ground (current flows out here).

    Phase floating_phase;                                 // Phase tri-stated - shows back-EMF, not driven.
};


// Standard six-step sequence for forward rotation. Walking this table
// forwards spins one way; walking it backwards spins the other. Each full
// pass through all six entries is one ELECTRICAL revolution, which equals
// one mechanical revolution divided by the motor's pole-pair count.
inline constexpr std::array<CommutationStep, 6U> kCommutationTable{{

    {Phase::kU, Phase::kV, Phase::kW},                    // Step 0: U driven, V grounded, W floating.

    {Phase::kU, Phase::kW, Phase::kV},                    // Step 1: U driven, W grounded, V floating.

    {Phase::kV, Phase::kW, Phase::kU},                    // Step 2: V driven, W grounded, U floating.

    {Phase::kV, Phase::kU, Phase::kW},                    // Step 3: V driven, U grounded, W floating.

    {Phase::kW, Phase::kU, Phase::kV},                    // Step 4: W driven, U grounded, V floating.

    {Phase::kW, Phase::kV, Phase::kU},                    // Step 5: W driven, V grounded, U floating.
}};


inline constexpr uint8_t kStepsPerElectricalRev = 6U;     // Number of commutation steps in one electrical revolution.


}  // namespace motor_control

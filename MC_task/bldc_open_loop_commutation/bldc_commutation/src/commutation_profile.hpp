#ifndef COMMUTATION_PROFILE_HPP                                    // Include guard opens.
#define COMMUTATION_PROFILE_HPP                                    // Include guard symbol.

#include <array>                                                   // std::array for the fixed six-step table.
#include <cstdint>                                                 // Fixed-width integer types.

// This header is deliberately free of any Zephyr dependency so the
// commutation table and the open-loop ramp maths can be compiled and
// unit-tested on the host with GTest (see tests/).

namespace bldc {

// ---------------------------------------------------------------------------
// Six-step (trapezoidal) commutation table
// ---------------------------------------------------------------------------

enum class PhaseState : uint8_t {                                  // What a single half-bridge must do in a step.
    Float = 0U,                                                    // Bridge disabled (SD asserted) -> phase is high-Z.
    High  = 1U,                                                    // Bridge enabled, IN driven with PWM -> sourcing.
    Low   = 2U                                                     // Bridge enabled, IN held at 0 -> sinking to ground.
};

struct CommutationStep {                                           // One entry of the six-step sequence.
    PhaseState phase_a;                                            // State of phase A half-bridge.
    PhaseState phase_b;                                            // State of phase B half-bridge.
    PhaseState phase_c;                                            // State of phase C half-bridge.
};

inline constexpr uint8_t kStepsPerElectricalRev = 6U;              // Six-step commutation -> 6 steps per electrical revolution.

// Standard 120-degree conduction sequence. In every step exactly one phase
// sources current, one sinks it, and one floats (the floating phase is where
// back-EMF would be measured in a sensorless closed-loop design).
inline constexpr std::array<CommutationStep, kStepsPerElectricalRev> kCommutationTable = {{
    {PhaseState::High,  PhaseState::Low,   PhaseState::Float},     // Step 0: A -> B.
    {PhaseState::High,  PhaseState::Float, PhaseState::Low  },     // Step 1: A -> C.
    {PhaseState::Float, PhaseState::High,  PhaseState::Low  },     // Step 2: B -> C.
    {PhaseState::Low,   PhaseState::High,  PhaseState::Float},     // Step 3: B -> A.
    {PhaseState::Low,   PhaseState::Float, PhaseState::High },     // Step 4: C -> A.
    {PhaseState::Float, PhaseState::Low,   PhaseState::High }      // Step 5: C -> B.
}};

// Returns the next step index, honouring the requested direction of rotation.
// Reversing the traversal order of the same table reverses the motor.
constexpr uint8_t next_step(uint8_t current_step, bool forward) noexcept {
    if (forward) {
        return static_cast<uint8_t>((current_step + 1U) % kStepsPerElectricalRev);
    }
    return static_cast<uint8_t>((current_step + (kStepsPerElectricalRev - 1U)) % kStepsPerElectricalRev);
}

// ---------------------------------------------------------------------------
// Open-loop ramp profile
// ---------------------------------------------------------------------------

// Tuning constants. These are the knobs to turn when a particular motor
// stalls on start-up or loses sync during the ramp.
inline constexpr uint32_t kAlignDurationMs      = 750U;            // Time spent parking the rotor on step 0 before ramping.
inline constexpr uint32_t kAlignDutyPercent     = 20U;             // Duty used during alignment (enough to pull the rotor in).

inline constexpr uint32_t kStartStepRateMilliHz = 50000U;          // 50 steps/s  -> 20.0 ms per step (slow crawl).
inline constexpr uint32_t kRunStepRateMilliHz   = 660000U;         // 660 steps/s -> ~1.52 ms per step (final cruise).
inline constexpr uint32_t kRampDurationMs       = 5000U;           // Time taken to climb from start rate to run rate.

inline constexpr uint32_t kStartDutyPercent     = 18U;             // Duty at the bottom of the ramp.
inline constexpr uint32_t kRunDutyPercent       = 45U;             // Duty at the top of the ramp.
inline constexpr uint32_t kMaxDutyPercent       = 60U;             // Hard ceiling; never exceeded regardless of tuning.

struct RampPoint {                                                 // Result of evaluating the ramp at one instant.
    uint32_t step_period_us;                                       // How long to dwell on the current commutation step.
    uint32_t duty_percent;                                         // PWM duty applied to the sourcing phase.
};

// Linear ramp in *commutation rate* (not in period), which gives constant
// angular acceleration rather than an acceleration that tails off. Duty is
// raised in step with the rate - a crude constant V/f law that keeps enough
// torque available as the electrical frequency climbs.
constexpr RampPoint evaluate_ramp(uint32_t elapsed_ms) noexcept {
    const uint32_t clamped_ms =
        (elapsed_ms > kRampDurationMs) ? kRampDurationMs : elapsed_ms;      // Saturate once the ramp completes.

    const uint64_t rate_span =
        static_cast<uint64_t>(kRunStepRateMilliHz - kStartStepRateMilliHz);  // Total rate increase across the ramp.

    const uint64_t rate_millihz =
        static_cast<uint64_t>(kStartStepRateMilliHz) +
        ((rate_span * static_cast<uint64_t>(clamped_ms)) / static_cast<uint64_t>(kRampDurationMs));
    // Interpolated commutation rate in milli-steps per second.

    const uint32_t period_us =
        static_cast<uint32_t>(1000000000ULL / rate_millihz);                 // Convert rate to a per-step dwell time.

    const uint64_t duty_span =
        static_cast<uint64_t>(kRunDutyPercent - kStartDutyPercent);          // Total duty increase across the ramp.

    uint32_t duty_percent =
        static_cast<uint32_t>(static_cast<uint64_t>(kStartDutyPercent) +
                              ((duty_span * static_cast<uint64_t>(clamped_ms)) /
                               static_cast<uint64_t>(kRampDurationMs)));
    // Interpolated duty, matched to the interpolated rate (V/f).

    if (duty_percent > kMaxDutyPercent) {                                    // Defensive clamp against mis-tuning.
        duty_percent = kMaxDutyPercent;
    }

    return RampPoint{period_us, duty_percent};
}

// Converts a duty percentage into a PWM pulse width for a given period.
constexpr uint32_t duty_to_pulse_ns(uint32_t period_ns, uint32_t duty_percent) noexcept {
    const uint32_t clamped =
        (duty_percent > kMaxDutyPercent) ? kMaxDutyPercent : duty_percent;   // Never let a bad caller over-drive the bridge.
    return static_cast<uint32_t>((static_cast<uint64_t>(period_ns) *
                                  static_cast<uint64_t>(clamped)) / 100ULL);
}

}  // namespace bldc

#endif  // COMMUTATION_PROFILE_HPP

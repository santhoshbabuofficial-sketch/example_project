#pragma once

#include <cstdint>                                        // Includes fixed-size integer types.


namespace motor_control {                              // Opens motor control task namespace.


enum class RampPhase : uint8_t {                          // Creates enum describing where the ramp currently is.

    kAlign      = 0U,                                     // Holding one step to park the rotor before spinning.

    kAccelerate = 1U,                                     // Stepping faster and faster - the gradual speed gain.

    kRunning    = 2U,                                     // Final speed reached; running continuously.
};


// Produces the open-loop speed profile: start slow, gain speed gradually,
// then hold final speed indefinitely.
//
// The two things that ramp together are the STEP PERIOD (how long each of
// the six steps is held - shorter means faster) and the DUTY CYCLE. They
// must rise together: a BLDC at low speed generates almost no back-EMF, so
// full duty at low speed is nearly a short across the winding. Raising
// duty in step with speed is the open-loop stand-in for proper V/f
// control, and is what keeps the current sane during the ramp.
class SpeedRamp {                                         // Creates class producing the open-loop speed profile.

public:

    SpeedRamp() = default;                                // Creates default constructor for SpeedRamp object.

    void Start();                                         // Resets the ramp and records its start time.

    // Recomputes the profile for the current moment. Call once per
    // commutation step, before applying that step.
    void Update();

    uint32_t step_period_us() const { return step_period_us_; } // Returns how long to hold the current step.

    uint32_t duty_percent() const { return duty_percent_; }     // Returns the duty cycle for the current step.

    RampPhase phase() const { return phase_; }                   // Returns which part of the profile is active.

    uint32_t ElectricalRpm() const;                              // Returns the present electrical speed, for logging.


    // ---- Profile tuning - placeholders, adjust for your motor and load ----

    static constexpr uint32_t kAlignDurationMs   = 1000U;
    // Holds step 0 for one second at align duty so the rotor is parked at
    // a known electrical position. Without this the first few commutations
    // fight whatever position the rotor happened to stop in, and the motor
    // stutters or kicks backwards before it catches.

    static constexpr uint32_t kAlignDutyPercent  = 15U;           // Duty used while parking the rotor.

    static constexpr uint32_t kRampDurationMs    = 8000U;         // Time taken to accelerate from start to final speed.

    static constexpr uint32_t kStartStepPeriodUs = 20000U;        // Slow first steps (20 ms per step).

    static constexpr uint32_t kFinalStepPeriodUs = 1500U;         // Final speed step period (1.5 ms per step).

    static constexpr uint32_t kStartDutyPercent  = 15U;           // Duty at the start of the ramp.

    static constexpr uint32_t kFinalDutyPercent  = 40U;           // Duty once final speed is reached.


private:

    int64_t  start_uptime_ms_ = 0;                                 // Stores the uptime at which the ramp started.

    uint32_t step_period_us_  = kStartStepPeriodUs;                // Stores the present step period in microseconds.

    uint32_t duty_percent_    = kAlignDutyPercent;                 // Stores the present duty cycle percentage.

    RampPhase phase_          = RampPhase::kAlign;                 // Stores which part of the profile is active.
};


}  // namespace motor_control

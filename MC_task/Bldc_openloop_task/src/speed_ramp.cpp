#include "speed_ramp.hpp"                                          // Includes SpeedRamp class definition.

#include <zephyr/kernel.h>                                         // Includes Zephyr uptime functions.
#include <zephyr/logging/log.h>                                    // Includes Zephyr logging functions.

#include "commutation_table.hpp"                                   // Includes kStepsPerElectricalRev.


LOG_MODULE_REGISTER(speed_ramp, LOG_LEVEL_INF);                    // Creates log module for the speed ramp.


namespace motor_control {                                     // Opens motor control task namespace.


void SpeedRamp::Start() {                                           // Resets the ramp and records its start time.

    start_uptime_ms_ = k_uptime_get();                              // Stores the moment the profile began.

    step_period_us_ = kStartStepPeriodUs;                           // Starts at the slowest step period.

    duty_percent_ = kAlignDutyPercent;                              // Starts at the low align duty.

    phase_ = RampPhase::kAlign;                                     // Begins in the rotor-align phase.

    LOG_INF("Ramp start: align %u ms, then %u ms accel to %u us/step",
            kAlignDurationMs, kRampDurationMs, kFinalStepPeriodUs);
    // Prints the profile actually compiled in.
}


void SpeedRamp::Update() {                                          // Recomputes the profile for the current moment.

    const int64_t elapsed_ms = k_uptime_get() - start_uptime_ms_;   // Measures how long the profile has been running.

    // ---- Phase 1: align --------------------------------------------------
    if (elapsed_ms < static_cast<int64_t>(kAlignDurationMs)) {      // Checks whether still parking the rotor.

        if (phase_ != RampPhase::kAlign) {                          // Checks whether the phase is being entered.

            phase_ = RampPhase::kAlign;                             // Records the align phase.
        }

        step_period_us_ = kStartStepPeriodUs;                        // Holds the slowest step period while aligning.

        duty_percent_ = kAlignDutyPercent;                           // Holds the low align duty.

        return;                                                      // Nothing further to compute.
    }

    const int64_t ramp_elapsed_ms =
        elapsed_ms - static_cast<int64_t>(kAlignDurationMs);         // Measures time spent in the acceleration phase.

    // ---- Phase 3: running at final speed -----------------------------------
    if (ramp_elapsed_ms >= static_cast<int64_t>(kRampDurationMs)) {  // Checks whether the ramp has completed.

        if (phase_ != RampPhase::kRunning) {                          // Checks whether final speed was just reached.

            phase_ = RampPhase::kRunning;                             // Records the running phase.

            LOG_INF("Final speed reached: %u us/step, %u%% duty, %u electrical RPM",
                    kFinalStepPeriodUs, kFinalDutyPercent, ElectricalRpm());
            // Prints the steady-state operating point once.
        }

        step_period_us_ = kFinalStepPeriodUs;                          // Holds the final step period indefinitely.

        duty_percent_ = kFinalDutyPercent;                             // Holds the final duty indefinitely.

        return;                                                        // Nothing further to compute.
    }

    // ---- Phase 2: accelerate ------------------------------------------------
    if (phase_ != RampPhase::kAccelerate) {                            // Checks whether acceleration is being entered.

        phase_ = RampPhase::kAccelerate;                               // Records the acceleration phase.

        LOG_INF("Align complete - accelerating");                      // Prints the transition out of align.
    }

    // Linear interpolation across the ramp, expressed in parts-per-thousand
    // so the whole profile stays in integer arithmetic. Integer math here
    // keeps the commutation path free of any floating-point work, which
    // matters because this runs once per step and the step period gets
    // down to ~1.5 ms.
    const uint32_t progress_permille = static_cast<uint32_t>(
        (ramp_elapsed_ms * 1000) / static_cast<int64_t>(kRampDurationMs));
    // Computes how far through the ramp we are, 0 to 1000.

    const uint32_t period_span = kStartStepPeriodUs - kFinalStepPeriodUs;
    // Total reduction in step period across the whole ramp.

    step_period_us_ =
        kStartStepPeriodUs - ((period_span * progress_permille) / 1000U);
    // Shortens the step period linearly - this is the speed gain.

    const uint32_t duty_span = kFinalDutyPercent - kStartDutyPercent;
    // Total increase in duty across the whole ramp.

    duty_percent_ =
        kStartDutyPercent + ((duty_span * progress_permille) / 1000U);
    // Raises duty in step with speed, keeping winding current reasonable.
}


uint32_t SpeedRamp::ElectricalRpm() const {                            // Returns the present electrical speed.

    if (step_period_us_ == 0U) {                                       // Guards against a divide by zero.

        return 0U;                                                     // Returns zero if the period is invalid.
    }

    // One electrical revolution takes six steps. Electrical RPM is
    // therefore 60 seconds / (6 * step_period). Divide by the motor's
    // pole-pair count to get MECHANICAL RPM - a 7-pole-pair hobby motor
    // spins at one seventh of the number reported here.
    const uint32_t electrical_rev_period_us =
        step_period_us_ * kStepsPerElectricalRev;                      // Duration of one electrical revolution.

    return (60U * 1000000U) / electrical_rev_period_us;                // Converts to revolutions per minute.
}


}  // namespace motor_control

#include <cstdint>                                                 // Fixed-width integer types.

#include <zephyr/kernel.h>                                         // Threads, timers, sleep.
#include <zephyr/logging/log.h>                                    // Logging macros.

#include "bldc_driver.hpp"                                         // Inverter hardware abstraction.
#include "commutation_profile.hpp"                                 // Step table and open-loop ramp maths.

LOG_MODULE_REGISTER(bldc_open_loop, LOG_LEVEL_INF);                // Log module for the application layer.

namespace {

// ---------------------------------------------------------------------------
// Static resources - nothing here is heap allocated
// ---------------------------------------------------------------------------

constexpr size_t kCommutationStackSize = 1024U;                    // Stack for the commutation thread.
constexpr int    kCommutationPriority  = K_HIGHEST_APPLICATION_THREAD_PRIO;  // Highest app priority: timing matters most.

K_THREAD_STACK_DEFINE(commutation_stack, kCommutationStackSize);   // Statically allocated thread stack.

k_thread commutation_thread_data{};                                // Statically allocated thread control block.

K_TIMER_DEFINE(commutation_timer, nullptr, nullptr);               // Paces commutation without cumulative drift.

bldc::BldcDriver driver{};                                         // The one and only inverter instance.

constexpr bool     kForwardRotation = true;                        // Direction: true walks the step table forwards.
constexpr uint32_t kLogIntervalMs   = 1000U;                       // How often the run state prints telemetry.

// ---------------------------------------------------------------------------
// Open-loop control states
// ---------------------------------------------------------------------------

enum class MotorState : uint8_t {
    Align = 0U,                                                    // Park the rotor on a known step.
    Ramp  = 1U,                                                    // Accelerate from crawl to cruise.
    Run   = 2U,                                                    // Hold the cruise rate indefinitely.
    Fault = 3U                                                     // Something failed; bridges are off.
};

// ---------------------------------------------------------------------------
// Commutation thread
// ---------------------------------------------------------------------------

void commutation_thread_entry(void* /*p1*/, void* /*p2*/, void* /*p3*/) {

    MotorState state = MotorState::Align;                          // Start by aligning the rotor.
    uint8_t    step_index = 0U;                                    // Current index into the six-step table.
    uint32_t   ramp_elapsed_ms = 0U;                               // Time spent ramping so far.
    uint32_t   log_accumulator_ms = 0U;                            // Throttles logging in the Run state.
    uint32_t   electrical_revs = 0U;                               // Completed electrical revolutions, for telemetry.

    // --- Alignment ---------------------------------------------------------
    // Energise a single step and hold it. The rotor snaps to that electrical
    // position, which gives the blind ramp a known starting angle. Without
    // this the first few commutations are a coin flip and the motor may
    // stutter or briefly run backwards.
    LOG_INF("state: ALIGN (%u ms @ %u%% duty)", bldc::kAlignDurationMs, bldc::kAlignDutyPercent);

    if (driver.apply_step(step_index, bldc::kAlignDutyPercent) != 0) {
        state = MotorState::Fault;                                 // Give up before spinning anything.
    } else {
        k_msleep(static_cast<int32_t>(bldc::kAlignDurationMs));    // Hold the alignment step.
        state = MotorState::Ramp;
        LOG_INF("state: RAMP (%u ms, %u -> %u steps/s)",
                bldc::kRampDurationMs,
                bldc::kStartStepRateMilliHz / 1000U,
                bldc::kRunStepRateMilliHz / 1000U);
    }

    // --- Commutation loop --------------------------------------------------
    while (state != MotorState::Fault) {

        const bldc::RampPoint point = bldc::evaluate_ramp(ramp_elapsed_ms);  // Rate and duty for right now.

        // Mark the top of every electrical revolution on the sync pin so a
        // scope can trigger on a repeatable point in the sequence.
        driver.pulse_sync(step_index == 0U);

        if (driver.apply_step(step_index, point.duty_percent) != 0) {        // Drive the bridges.
            state = MotorState::Fault;
            break;
        }

        // Arm a one-shot timer for the dwell time, then block on it. Using the
        // timer rather than k_sleep() keeps the commutation period referenced
        // to the kernel clock, so scheduling jitter does not accumulate into
        // a slowly drifting electrical frequency.
        k_timer_start(&commutation_timer, K_USEC(point.step_period_us), K_NO_WAIT);
        (void)k_timer_status_sync(&commutation_timer);                        // Sleep until this step's dwell expires.

        step_index = bldc::next_step(step_index, kForwardRotation);           // Advance the sequence.

        if (step_index == 0U) {                                               // Wrapped -> one electrical revolution done.
            electrical_revs++;
        }

        const uint32_t elapsed_step_ms = point.step_period_us / 1000U;        // Approximate ms consumed by this step.

        if (state == MotorState::Ramp) {
            ramp_elapsed_ms += elapsed_step_ms;                               // Advance along the ramp.

            if (ramp_elapsed_ms >= bldc::kRampDurationMs) {                   // Ramp finished.
                ramp_elapsed_ms = bldc::kRampDurationMs;                      // Pin it so evaluate_ramp() saturates.
                state = MotorState::Run;
                LOG_INF("state: RUN (steady at %u us/step, %u%% duty)",
                        point.step_period_us, point.duty_percent);
            }
        } else {
            log_accumulator_ms += elapsed_step_ms;                            // Run state: periodic telemetry only.

            if (log_accumulator_ms >= kLogIntervalMs) {
                log_accumulator_ms = 0U;
                LOG_INF("running: %u us/step, %u%% duty, %u elec revs",
                        point.step_period_us, point.duty_percent, electrical_revs);
            }
        }
    }

    // --- Fault -------------------------------------------------------------
    driver.coast();                                                           // Bridges off, windings free.
    driver.pulse_sync(false);                                                 // Leave the trigger line idle.
    LOG_ERR("state: FAULT - inverter disabled, motor coasting");

    while (true) {                                                            // Park the thread; require a reset to recover.
        k_msleep(1000);
    }
}

}  // namespace

int main() {

    LOG_INF("open-loop BLDC commutation starting (NUCLEO-G474RE / TIM1)");

    if (driver.init() != 0) {                                                 // Validate PWM channels and GPIOs.
        LOG_ERR("inverter init failed - not spinning the motor");
        return -1;                                                            // Nothing is ever energised on this path.
    }

    const k_tid_t tid = k_thread_create(&commutation_thread_data,             // Spawn the timing-critical thread.
                                        commutation_stack,
                                        K_THREAD_STACK_SIZEOF(commutation_stack),
                                        commutation_thread_entry,
                                        nullptr, nullptr, nullptr,
                                        kCommutationPriority,
                                        0,
                                        K_NO_WAIT);

    (void)k_thread_name_set(tid, "commutation");                              // Name it for shell / thread analyzer output.

    while (true) {                                                            // Main thread stays idle; commutation runs elsewhere.
        k_msleep(1000);
    }

    return 0;                                                                 // Unreachable.
}

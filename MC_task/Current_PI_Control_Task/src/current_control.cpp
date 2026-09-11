#include "current_control.hpp"                                       // Includes CurrentLoopController class definition.

#include <zephyr/drivers/gpio.h>                                    // Includes Zephyr GPIO control functions.
#include <zephyr/drivers/pwm.h>                                     // Includes Zephyr PWM control functions.
#include <zephyr/logging/log.h>                                     // Includes Zephyr logging functions.


LOG_MODULE_REGISTER(current_control, LOG_LEVEL_INF);                // Creates log module for the current loop.


namespace motor_control {                                      // Opens motor control namespace.


CurrentLoopController::CurrentLoopController(GpioOverlay& overlay, ICurrentSensor& current_sensor,
                                              PiController& pi)
    : overlay_(overlay), current_sensor_(current_sensor), pi_(pi) {}
// Constructor wires the controller to hardware, the sensor, and the PI core.


bool CurrentLoopController::Init() {                                  // Brings the coil driver to a safe stopped state.

    gpio_pin_set_dt(&overlay_.enable_pin(), 0);                        // Asserts SD (shutdown) - driver disabled at init.

    duty_percent_ = kDutyMinPercent;                                    // Starts commanded duty at 0% while the driver is disabled.

    ApplyDutyPercent(duty_percent_);                                     // Pushes 0% duty out to the PWM leg.

    pi_.Reset();                                                          // Clears any stale PI integral state.

    enabled_ = false;                                                     // Starts disabled - Enable() must be called explicitly.

    return true;                                                          // Returns successful initialization.
}


void CurrentLoopController::ApplyDutyPercent(uint8_t duty_percent) {       // Pushes a duty percentage out to the PWM leg.

    const uint32_t pulse_ns =
        (static_cast<uint32_t>(GpioOverlay::kPwmPeriodNs) * duty_percent) / 100U;
    // Converts the commanded percentage into a pulse width in nanoseconds.

    pwm_set(overlay_.pwm_dev(), GpioOverlay::kPwmChannel, GpioOverlay::kPwmPeriodNs, pulse_ns,
            PWM_POLARITY_NORMAL);
    // Applies the coil-leg (TIM1_CH1/PC0) PWM duty cycle.
}


void CurrentLoopController::SetReferenceCurrent(float reference_a) {         // Sets the target coil current.

    reference_a_ = reference_a;                                               // Stores the new reference current.

    LOG_INF("Reference current set to %d.%02u A", static_cast<int>(reference_a_),
            static_cast<unsigned>((reference_a_ - static_cast<int>(reference_a_)) * 100));
    // Prints the new reference for tuning/step-response visibility.
}


void CurrentLoopController::Enable() {                                        // Enables the coil driver and starts regulation.

    duty_percent_ = kStartDutyPercent;                                            // Opens the loop at the fixed starting duty (50%).

    ApplyDutyPercent(duty_percent_);                                               // Pushes the starting duty out to the PWM leg immediately.

    pi_.Reset(static_cast<float>(kStartDutyPercent));
    // Seeds the PI integral so the very first Tick() output starts at
    // kStartDutyPercent (for zero error) rather than ramping up from zero -
    // if measured current is already above the reference at that duty, the
    // very first control step pulls the duty back down straight away.

    gpio_pin_set_dt(&overlay_.enable_pin(), 1);                                  // Clears SD (active-LOW spec: 1 == enabled).

    enabled_ = true;                                                              // Marks the loop as actively regulating.

    LOG_INF("Current loop ENABLED at %u%% starting duty", duty_percent_);           // Logs the enable event.
}


void CurrentLoopController::Disable() {                                          // Disables the coil driver (normal stop).

    duty_percent_ = kDutyMinPercent;                                               // Clears commanded duty.

    ApplyDutyPercent(duty_percent_);                                                // Applies 0% duty before disabling the driver.

    gpio_pin_set_dt(&overlay_.enable_pin(), 0);                                     // Asserts SD (shutdown).

    enabled_ = false;                                                                // Marks the loop as stopped.

    LOG_INF("Current loop DISABLED");                                                // Logs the disable event.
}


void CurrentLoopController::EmergencyStop() {                                        // Immediately halts the coil driver (fault path).

    duty_percent_ = kDutyMinPercent;                                                    // Clears commanded duty.

    ApplyDutyPercent(duty_percent_);                                                     // Applies 0% duty immediately.

    gpio_pin_set_dt(&overlay_.enable_pin(), 0);                                            // Asserts SD (shutdown) immediately.

    enabled_ = false;                                                                        // Marks the loop as stopped.

    pi_.Reset();                                                                              // Clears integral state so a restart begins clean.

    LOG_ERR("Current loop EMERGENCY STOP");                                                     // Logs the emergency stop.
}


void CurrentLoopController::Tick(float dt_s) {                                                  // Runs one control-loop iteration.

    if (!enabled_) {                                                                              // Skips regulation while disabled.

        return;                                                                                    // Nothing to do.
    }

    float measured = 0.0F;                                                                          // Creates local buffer for this sample's measured current.

    if (!current_sensor_.Read(measured)) {                                                           // Reads the coil current.

        LOG_WRN("Current sensor read failed - holding last duty");                                    // Prints sensor-read warning.

        return;                                                                                        // Skips this control step; keeps last commanded duty applied.
    }

    measured_a_ = measured;                                                                            // Stores the fresh measurement.

    const float pi_output_percent = pi_.Update(reference_a_, measured_a_, dt_s);                         // Runs one PI step (setpoint - measurement -> duty%).

    float clamped_percent = pi_output_percent;                                                            // Creates a working copy to clamp into range.

    if (clamped_percent < static_cast<float>(kDutyMinPercent)) {                                           // Checks lower duty bound.

        clamped_percent = static_cast<float>(kDutyMinPercent);                                              // Clamps to the minimum duty.

    } else if (clamped_percent > static_cast<float>(kDutyMaxPercent)) {                                     // Checks upper duty bound.

        clamped_percent = static_cast<float>(kDutyMaxPercent);                                               // Clamps to the maximum duty.
    }

    duty_percent_ = static_cast<uint8_t>(clamped_percent);                                                   // Stores the applied duty percentage.

    ApplyDutyPercent(duty_percent_);                                                                          // Pushes the new duty out to the PWM leg.

    LOG_INF("ref=%d.%02uA meas=%d.%02uA err=%d.%02uA duty=%u%%",
            static_cast<int>(reference_a_),
            static_cast<unsigned>((reference_a_ - static_cast<int>(reference_a_)) * 100),
            static_cast<int>(measured_a_),
            static_cast<unsigned>((measured_a_ - static_cast<int>(measured_a_)) * 100),
            static_cast<int>(pi_.last_error()),
            static_cast<unsigned>((pi_.last_error() - static_cast<int>(pi_.last_error())) * 100),
            duty_percent_);
    // Prints per-cycle telemetry so the PI step response can be observed on
    // the console (this replaces the LCD row-1 status readout from the
    // direction-reversal task, which this task does not use).
}


}  // namespace motor_control

#include "motor_control.hpp"                                        // Includes MotorControl class definition.

#include <zephyr/drivers/gpio.h>                                    // Includes Zephyr GPIO control functions.
#include <zephyr/drivers/pwm.h>                                     // Includes Zephyr PWM control functions.
#include <zephyr/logging/log.h>                                     // Includes Zephyr logging functions.


LOG_MODULE_REGISTER(motor_control, LOG_LEVEL_INF);                  // Creates log module for motor control.


namespace mc::motor_control {                                      // Opens motor control namespace.


MotorControl::MotorControl(GpioOverlay& overlay) : overlay_(overlay) {}
// Constructor connects motor control object with GPIO/PWM hardware control.


bool MotorControl::Init() {                                          // Initializes motor control hardware to a safe stopped state.

    DisableBridges();                                                 // Keeps both half-bridges disabled at power-up.

    SetDirectionIndicators(Direction::kForward);                       // Clears both direction indicator pins.

    pwm_percent_ = kPwmMinPercent;                                     // Starts commanded duty at 0%.

    ApplyPwm();                                                        // Pushes 0% duty out to both PWM legs.

    ramp_state_ = RampState::kStopped;                                  // Starts in the stopped ramp state.

    running_ = false;                                                   // Starts not-running.

    faulted_ = false;                                                    // Starts fault-free.

    return true;                                                         // Returns successful initialization.
}


void MotorControl::ApplyPwm() {                                         // Pushes current duty/direction out to both PWM legs.

    const uint32_t pulse_ns =
        (static_cast<uint32_t>(GpioOverlay::kPwmPeriodNs) * pwm_percent_) / 100U;
    // Converts the commanded percentage into a pulse width in nanoseconds.

    const uint32_t pwm1_pulse_ns = (direction_ == Direction::kForward) ? pulse_ns : 0U;
    // Leg 1 (IR2104 #1) carries the duty cycle only when driving forward.

    const uint32_t pwm2_pulse_ns = (direction_ == Direction::kReverse) ? pulse_ns : 0U;
    // Leg 2 (IR2104 #2) carries the duty cycle only when driving reverse.

    pwm_set(overlay_.pwm_dev(), GpioOverlay::kPwm1Channel, GpioOverlay::kPwmPeriodNs,
            pwm1_pulse_ns, PWM_POLARITY_NORMAL);
    // Applies leg 1 (TIM1_CH1/PC0) PWM duty cycle.

    pwm_set(overlay_.pwm_dev(), GpioOverlay::kPwm2Channel, GpioOverlay::kPwmPeriodNs,
            pwm2_pulse_ns, PWM_POLARITY_NORMAL);
    // Applies leg 2 (TIM1_CH3/PC2) PWM duty cycle - same TIM1 device, different channel.
}


void MotorControl::SetDirectionIndicators(Direction dir) {              // Drives the PB13/PB15 indicator pins.

    const bool fwd = (dir == Direction::kForward);                       // Checks if direction is forward.

    gpio_pin_set_dt(&overlay_.dir_fwd(), fwd ? 1 : 0);                    // Sets PB13 high only for forward.

    gpio_pin_set_dt(&overlay_.dir_rev(), fwd ? 0 : 1);                     // Sets PB15 high only for reverse.
}


void MotorControl::EnableBridges() {                                       // Enables both IR2104 half-bridges.

    gpio_pin_set_dt(&overlay_.sd1(), 1);                                    // Clears SD1 (active-LOW spec: 1 == enabled).

    gpio_pin_set_dt(&overlay_.sd2(), 1);                                     // Clears SD2 (active-LOW spec: 1 == enabled).
}


void MotorControl::DisableBridges() {                                       // Disables both IR2104 half-bridges.

    gpio_pin_set_dt(&overlay_.sd1(), 0);                                     // Asserts SD1 shutdown (active-LOW spec: 0 == disabled).

    gpio_pin_set_dt(&overlay_.sd2(), 0);                                      // Asserts SD2 shutdown (active-LOW spec: 0 == disabled).
}


void MotorControl::OnButtonPress() {                                          // Handles one debounced button press.

    if (faulted_) {                                                            // Ignores button presses while a fault is latched.

        LOG_WRN("Button press ignored - motor is in FAULT state");             // Prints ignored-press notice.

        return;                                                                 // Does nothing further.
    }

    if (!running_) {                                                            // Checks whether this is the very first press.

        running_ = true;                                                         // Marks the motor as running.

        direction_ = Direction::kForward;                                         // Always starts in forward direction.

        ramp_state_ = RampState::kIncreasing;                                       // Begins ramping up.

        pwm_percent_ = kPwmStepPercent;                                             // Starts at the first step (5%).

        SetDirectionIndicators(direction_);                                          // Updates PB13/PB15 for forward.

        EnableBridges();                                                             // Enables both half-bridges.

        ApplyPwm();                                                                   // Applies the initial 5% duty.

        LOG_INF("Motor START: direction=FWD pwm=%u%%", pwm_percent_);                  // Logs motor start.

        return;                                                                        // Done handling this press.
    }

    // Motor is already running: a press flips the ramp direction. Reaching
    // 0% while decreasing is handled in Tick(), which then auto-reverses.
    if (ramp_state_ == RampState::kIncreasing || ramp_state_ == RampState::kHoldAtMax) {

        ramp_state_ = RampState::kDecreasing;                                           // Switches to decelerating.

        LOG_INF("Motor DECEL requested at pwm=%u%%", pwm_percent_);                       // Logs deceleration request.

    } else if (ramp_state_ == RampState::kDecreasing) {

        ramp_state_ = RampState::kIncreasing;                                              // Switches back to accelerating.

        LOG_INF("Motor ACCEL requested at pwm=%u%%", pwm_percent_);                          // Logs acceleration request.
    }
}


void MotorControl::Tick() {                                                                    // Advances the ramp by one step.

    if (!running_ || faulted_) {                                                                // Skips if stopped or faulted.

        return;                                                                                  // Nothing to do.
    }

    if (ramp_state_ == RampState::kIncreasing) {                                                  // Handles the accelerate phase.

        if (pwm_percent_ + kPwmStepPercent >= kPwmMaxPercent) {                                     // Checks if this step reaches max.

            pwm_percent_ = kPwmMaxPercent;                                                            // Clamps duty at 100%.

            ramp_state_ = RampState::kHoldAtMax;                                                        // Holds at max until next press.

        } else {

            pwm_percent_ = static_cast<uint8_t>(pwm_percent_ + kPwmStepPercent);                          // Steps duty up by 5%.
        }

        ApplyPwm();                                                                                        // Applies the updated duty.

    } else if (ramp_state_ == RampState::kDecreasing) {                                                     // Handles the decelerate phase.

        if (pwm_percent_ <= kPwmStepPercent) {                                                                // Checks if this step reaches zero.

            pwm_percent_ = kPwmMinPercent;                                                                      // Clamps duty at 0%.

            ApplyPwm();                                                                                          // Applies zero duty before flipping direction.

            direction_ = (direction_ == Direction::kForward) ? Direction::kReverse : Direction::kForward;         // Flips direction.

            SetDirectionIndicators(direction_);                                                                     // Updates PB13/PB15 for the new direction.

            LOG_INF("Motor direction reversed -> %s", direction_ == Direction::kForward ? "FWD" : "REV");            // Logs reversal.

            ramp_state_ = RampState::kIncreasing;                                                                      // Resumes ramping up in the new direction.

            pwm_percent_ = kPwmStepPercent;                                                                              // Starts the new direction at 5%.

            ApplyPwm();                                                                                                   // Applies the new-direction starting duty.

        } else {

            pwm_percent_ = static_cast<uint8_t>(pwm_percent_ - kPwmStepPercent);                                            // Steps duty down by 5%.

            ApplyPwm();                                                                                                      // Applies the updated duty.
        }
    }

    // kStopped and kHoldAtMax phases require no periodic action.
}


void MotorControl::EmergencyStop() {                                                                                          // Immediately halts the motor (fault path).

    DisableBridges();                                                                                                          // Disables both half-bridges immediately.

    pwm_percent_ = kPwmMinPercent;                                                                                              // Clears commanded duty.

    ApplyPwm();                                                                                                                  // Applies zero duty to both legs.

    ramp_state_ = RampState::kStopped;                                                                                            // Halts the ramp state machine.

    running_ = false;                                                                                                              // Marks the motor as stopped.

    faulted_ = true;                                                                                                                // Latches the fault flag.

    LOG_ERR("Motor EMERGENCY STOP");                                                                                                 // Logs the emergency stop.
}


void MotorControl::ClearFaultAndStop() {                                                                                              // Clears fault and leaves motor stopped.

    faulted_ = false;                                                                                                                  // Clears the latched fault flag.

    ramp_state_ = RampState::kStopped;                                                                                                   // Leaves the ramp state machine stopped.

    running_ = false;                                                                                                                     // Leaves the motor not-running; next button press restarts it.
}


}  // namespace mc::motor_control                                                                                                        // Closes motor control namespace.

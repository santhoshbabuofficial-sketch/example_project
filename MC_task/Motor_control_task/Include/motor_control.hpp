#pragma once                                              // Prevents this header file from being included multiple times.

#include <cstdint>                                        // Includes fixed-size integer types.

#include "gpio_overlay.hpp"                                // Includes GPIO/PWM hardware access.


namespace mc::motor_control {                              // Opens motor control namespace.


enum class Direction {                                      // Enumerates motor rotation direction.

    kForward,                                                // Motor commanded to rotate forward.
    kReverse,                                                 // Motor commanded to rotate reverse.
};


enum class RampState {                                       // Enumerates PWM ramp phase.

    kStopped,                                                 // Motor has never been started (PWM = 0, bridges disabled).
    kIncreasing,                                               // PWM ramping up towards 100%.
    kHoldAtMax,                                                // PWM has reached 100% and is holding there.
    kDecreasing,                                               // PWM ramping down towards 0%, then auto-reverses.
};


// Drives the H-bridge (2x IR2104 half-bridge) using a "PWM leg + static leg"
// scheme: whichever leg matches the current direction carries the duty
// cycle, the other leg is held at 0%. Speed ramps up/down in 5% steps on
// each button press per the task spec; reaching 0% while decelerating
// automatically flips direction and resumes ramping up.
class MotorControl {                                          // Creates class implementing the motor control state machine.

public:

    explicit MotorControl(GpioOverlay& overlay);               // Constructor connects motor control with GPIO/PWM hardware.

    bool Init();                                                // Initializes PWM outputs and brings hardware to a safe stopped state.

    void OnButtonPress();                                       // Handles one debounced user-button press event.

    void Tick();                                                // Advances the ramp state machine by one step (call periodically).

    void EmergencyStop();                                       // Immediately disables both bridges and halts the ramp (fault path).

    void ClearFaultAndStop();                                   // Clears the faulted flag and leaves the motor in a stopped state.

    uint8_t pwm_percent() const { return pwm_percent_; }        // Returns current commanded PWM duty percentage (0-100).

    Direction direction() const { return direction_; }          // Returns current commanded direction.

    bool is_running() const { return running_; }                // Returns whether the motor is actively running (not stopped/faulted).

    bool is_faulted() const { return faulted_; }                 // Returns whether the motor is latched in a fault state.


private:

    static constexpr uint8_t kPwmStepPercent = 5U;               // PWM ramp step size, per task spec (5% steps).
    static constexpr uint8_t kPwmMaxPercent = 100U;               // Maximum PWM duty percentage.
    static constexpr uint8_t kPwmMinPercent = 0U;                  // Minimum PWM duty percentage.

    void ApplyPwm();                                              // Pushes pwm_percent_/direction_ out to the two PWM legs.

    void SetDirectionIndicators(Direction dir);                    // Drives the PB13/PB15 direction indicator pins.

    void EnableBridges();                                          // Enables both IR2104 half-bridges (clears SD/shutdown).

    void DisableBridges();                                         // Disables both IR2104 half-bridges (asserts SD/shutdown).

    GpioOverlay& overlay_;                                          // Stores reference to GPIO/PWM hardware control object.

    uint8_t pwm_percent_ = 0U;                                       // Stores current commanded PWM duty percentage.

    Direction direction_ = Direction::kForward;                       // Stores current commanded direction.

    RampState ramp_state_ = RampState::kStopped;                       // Stores current ramp phase.

    bool running_ = false;                                              // Stores whether the motor has been started.

    bool faulted_ = false;                                               // Stores whether a fault has latched the motor off.
};


}  // namespace mc::motor_control                                       // Closes motor control namespace.

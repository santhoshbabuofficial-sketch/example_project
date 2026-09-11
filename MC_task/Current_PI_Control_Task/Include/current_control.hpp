#pragma once

#include <cstdint>                                                // Includes fixed-size integer types.

#include "current_sensor.hpp"                                    // Includes current sensor interface.
#include "gpio_overlay.hpp"                                       // Includes GPIO/PWM hardware access.
#include "pi_controller.hpp"                                      // Includes generic PI controller.


namespace motor_control {                                    // Opens motor control namespace.


// Ties the three pieces of the current loop together each Tick():
//   measure actual coil current -> PI(reference, actual) -> apply duty.
// This is the "single phase/coil closed-loop current regulation" the task
// asks for, and is deliberately the same shape a single leg of an FOC
// current loop would take later.
class CurrentLoopController {                                  // Creates class implementing the closed-loop current controller.

public:

    CurrentLoopController(GpioOverlay& overlay, ICurrentSensor& current_sensor, PiController& pi);
    // Constructor wires the controller to hardware, the sensor, and the PI core.

    bool Init();                                                 // Brings PWM/enable pin to a safe stopped state.

    void SetReferenceCurrent(float reference_a);                  // Sets the target coil current in amperes.

    void Enable();                                                 // Enables the coil driver and starts closed-loop regulation.

    void Disable();                                                 // Disables the coil driver and holds 0% duty (normal stop).

    void EmergencyStop();                                           // Immediately disables the driver and latches a stop (fault path).

    // Runs one control-loop iteration: read sensor, run PI, apply duty.
    // dt_s is the time since the previous Tick() call, in seconds.
    void Tick(float dt_s);

    float reference_current() const { return reference_a_; }        // Returns the currently commanded reference current.

    float measured_current() const { return measured_a_; }          // Returns the most recently measured coil current.

    uint8_t duty_percent() const { return duty_percent_; }          // Returns the currently applied PWM duty percentage.

    bool is_enabled() const { return enabled_; }                    // Returns whether the loop is actively regulating.


private:

    static constexpr uint8_t kDutyMinPercent = 0U;                   // Minimum PWM duty percentage (PI output lower clamp).
    static constexpr uint8_t kDutyMaxPercent = 90U;                  // Maximum PWM duty percentage (leaves headroom off 100%).
    static constexpr uint8_t kStartDutyPercent = 50U;                 // Open-loop starting duty applied the instant the loop enables.

    void ApplyDutyPercent(uint8_t duty_percent);                      // Pushes a duty percentage out to the PWM peripheral.

    GpioOverlay& overlay_;                                             // Stores reference to GPIO/PWM hardware control object.

    ICurrentSensor& current_sensor_;                                    // Stores reference to the current sensor.

    PiController& pi_;                                                   // Stores reference to the PI controller core.

    float reference_a_ = 0.0F;                                            // Stores the commanded target coil current.

    float measured_a_ = 0.0F;                                              // Stores the most recently measured coil current.

    uint8_t duty_percent_ = 0U;                                             // Stores the currently applied PWM duty percentage.

    bool enabled_ = false;                                                   // Stores whether the loop is actively regulating.
};


}  // namespace motor_control

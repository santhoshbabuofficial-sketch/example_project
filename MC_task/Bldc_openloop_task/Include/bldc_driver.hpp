#pragma once

#include <cstdint>                                        // Includes fixed-size integer types.

#include "commutation_table.hpp"                          // Includes Phase and CommutationStep definitions.
#include "gpio_overlay.hpp"                               // Includes hardware access object.


namespace motor_control {                              // Opens motor control task namespace.


// Drives the three-phase inverter one commutation step at a time.
//
// HARDWARE ASSUMPTION - verify against your driver board: each phase is
// backed by a half-bridge driver (DRV8313 / L6234 class) that takes ONE
// PWM input to set high-side duty (low side and dead time handled
// internally) plus ONE enable pin to tri-state the phase. If you are
// instead driving discrete MOSFETs with separate high/low gate drivers
// (as the IR2104 pairs in the brushed-motor project do), only
// DriveHigh/DriveLow/FloatPhase below need rewriting - the commutation
// logic above them is unaffected.
class BldcDriver {                                        // Creates class that applies commutation steps to the inverter.

public:

    explicit BldcDriver(GpioOverlay& overlay);            // Constructor connects driver with GPIO/PWM hardware.

    bool Init();                                          // Initializes the inverter into a safe all-off state.

    // Applies one commutation step at the given duty cycle, and pulses the
    // scope sync outputs so the step boundary is visible on a scope.
    bool ApplyStep(uint8_t step_index, uint32_t duty_percent);

    void AllOff();                                        // Tri-states all three phases (coast / safe stop).


private:

    bool DriveHigh(Phase phase, uint32_t duty_percent);   // Enables a phase and applies PWM duty to it.

    bool DriveLow(Phase phase);                            // Enables a phase and holds it at 0% duty (grounded).

    bool FloatPhase(Phase phase);                          // Tri-states a phase so it floats (shows back-EMF).

    uint32_t ChannelFor(Phase phase) const;                // Maps a phase onto its TIM1 PWM channel number.

    const gpio_dt_spec& EnableFor(Phase phase) const;      // Maps a phase onto its enable pin spec.

    GpioOverlay& overlay_;                                  // Stores reference to GPIO/PWM hardware control object.

    bool initialized_ = false;                              // Stores whether inverter initialization is completed.
};


}  // namespace motor_control

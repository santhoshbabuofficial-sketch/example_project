#include "bldc_driver.hpp"                                        // Includes BldcDriver class definition.

#include <zephyr/drivers/pwm.h>                                   // Includes Zephyr PWM driver functions.
#include <zephyr/logging/log.h>                                   // Includes Zephyr logging functions.


LOG_MODULE_REGISTER(bldc_driver, LOG_LEVEL_INF);                  // Creates log module for the inverter driver.


namespace motor_control {                                    // Opens motor control task namespace.


BldcDriver::BldcDriver(GpioOverlay& overlay) : overlay_(overlay) {}
// Constructor connects the inverter driver with the GPIO/PWM hardware object.


bool BldcDriver::Init() {                                          // Initializes the inverter into a safe all-off state.

    if (overlay_.pwm_dev() == nullptr) {                           // Checks that the PWM device was resolved.

        LOG_ERR("BLDC driver: PWM device missing");                // Prints PWM device error.

        return false;                                               // Returns failure.
    }

    AllOff();                                                       // Tri-states every phase before anything spins.

    initialized_ = true;                                            // Marks inverter initialization as completed.

    return true;                                                    // Returns successful initialization.
}


uint32_t BldcDriver::ChannelFor(Phase phase) const {               // Maps a phase onto its TIM1 PWM channel.

    switch (phase) {

    case Phase::kU:  return GpioOverlay::kPhaseUChannel;           // Phase U uses TIM1_CH1.

    case Phase::kV:  return GpioOverlay::kPhaseVChannel;           // Phase V uses TIM1_CH2.

    case Phase::kW:
    default:         return GpioOverlay::kPhaseWChannel;           // Phase W uses TIM1_CH3.
    }
}


const gpio_dt_spec& BldcDriver::EnableFor(Phase phase) const {     // Maps a phase onto its enable pin spec.

    switch (phase) {

    case Phase::kU:  return overlay_.en_u();                       // Phase U enable pin.

    case Phase::kV:  return overlay_.en_v();                       // Phase V enable pin.

    case Phase::kW:
    default:         return overlay_.en_w();                       // Phase W enable pin.
    }
}


bool BldcDriver::DriveHigh(Phase phase, uint32_t duty_percent) {   // Enables a phase and applies PWM duty to it.

    const gpio_dt_spec& enable = EnableFor(phase);                 // Looks up this phase's enable pin.

    if (gpio_pin_set_dt(&enable, 1) != 0) {                        // Enables the phase's half-bridge output.

        return false;                                              // Returns failure if the pin write failed.
    }

    const uint32_t pulse_ns =
        (GpioOverlay::kPwmPeriodNs * duty_percent) / 100U;          // Converts duty percentage into pulse width.

    return pwm_set(overlay_.pwm_dev(), ChannelFor(phase),
                   GpioOverlay::kPwmPeriodNs, pulse_ns, 0) == 0;    // Applies the PWM duty to this phase.
}


bool BldcDriver::DriveLow(Phase phase) {                            // Enables a phase and holds it grounded.

    const gpio_dt_spec& enable = EnableFor(phase);                  // Looks up this phase's enable pin.

    if (gpio_pin_set_dt(&enable, 1) != 0) {                         // Enables the phase's half-bridge output.

        return false;                                               // Returns failure if the pin write failed.
    }

    // 0% duty with the phase enabled means the low-side device is on
    // continuously - this is the return path for the motor current.
    return pwm_set(overlay_.pwm_dev(), ChannelFor(phase),
                   GpioOverlay::kPwmPeriodNs, 0U, 0) == 0;          // Holds this phase at ground.
}


bool BldcDriver::FloatPhase(Phase phase) {                          // Tri-states a phase so it floats.

    // Duty is zeroed FIRST so nothing is being driven at the instant the
    // enable pin drops - avoids a momentary shoot-through-adjacent state
    // on drivers that latch their input on the enable edge.
    static_cast<void>(pwm_set(overlay_.pwm_dev(), ChannelFor(phase),
                              GpioOverlay::kPwmPeriodNs, 0U, 0));    // Zeroes this phase's duty first.

    const gpio_dt_spec& enable = EnableFor(phase);                   // Looks up this phase's enable pin.

    return gpio_pin_set_dt(&enable, 0) == 0;                         // Tri-states the phase.
}


bool BldcDriver::ApplyStep(uint8_t step_index, uint32_t duty_percent) {
    // Applies one commutation step and marks the boundary for the scope.

    if (!initialized_) {                                              // Checks whether the driver was initialized.

        return false;                                                 // Returns failure if not ready.
    }

    const CommutationStep& step =
        kCommutationTable[step_index % kStepsPerElectricalRev];        // Selects the step, wrapping at six.

    // ORDER MATTERS: float the outgoing phase before driving anything, so
    // no phase is ever briefly both driven and grounded during the
    // transition.
    if (!FloatPhase(step.floating_phase)) {                           // Tri-states the phase that should float.

        LOG_ERR("Commutation: failed to float phase");                // Prints float failure.

        return false;                                                 // Returns failure.
    }

    if (!DriveLow(step.low_phase)) {                                  // Grounds the current return phase.

        LOG_ERR("Commutation: failed to ground phase");               // Prints ground failure.

        return false;                                                 // Returns failure.
    }

    if (!DriveHigh(step.high_phase, duty_percent)) {                  // PWM-drives the source phase.

        LOG_ERR("Commutation: failed to drive phase");                // Prints drive failure.

        return false;                                                 // Returns failure.
    }

    // ---- Scope sync outputs ------------------------------------------------
    // step_sync toggles on every commutation, so its edges line up exactly
    // with the phase waveform transitions - handy as a scope trigger when
    // you want to catch one specific step.
    gpio_pin_toggle_dt(&overlay_.step_sync());                        // Toggles the per-step sync line.

    // cycle_sync goes HIGH only during step 0, giving one pulse per
    // ELECTRICAL revolution. Trigger the scope on this line to get a
    // stable picture of all six steps at once.
    const bool is_cycle_start =
        ((step_index % kStepsPerElectricalRev) == 0U);                 // Checks whether this is the first step.

    gpio_pin_set_dt(&overlay_.cycle_sync(), is_cycle_start ? 1 : 0);   // Marks the start of each electrical cycle.

    return true;                                                       // Returns successful commutation.
}


void BldcDriver::AllOff() {                                            // Tri-states all three phases.

    static_cast<void>(FloatPhase(Phase::kU));                          // Floats phase U.

    static_cast<void>(FloatPhase(Phase::kV));                          // Floats phase V.

    static_cast<void>(FloatPhase(Phase::kW));                          // Floats phase W.

    gpio_pin_set_dt(&overlay_.step_sync(), 0);                          // Clears the per-step sync line.

    gpio_pin_set_dt(&overlay_.cycle_sync(), 0);                         // Clears the per-cycle sync line.
}


}  // namespace motor_control

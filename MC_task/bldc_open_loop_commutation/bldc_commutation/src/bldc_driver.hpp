#ifndef BLDC_DRIVER_HPP                                            // Include guard opens.
#define BLDC_DRIVER_HPP                                            // Include guard symbol.

#include <cstdint>                                                 // Fixed-width integer types.

#include <zephyr/device.h>                                         // Zephyr device structure.
#include <zephyr/drivers/gpio.h>                                   // gpio_dt_spec for the bridge enable pins.
#include <zephyr/drivers/pwm.h>                                    // pwm_dt_spec / pwm_set for the phase inputs.

#include "commutation_profile.hpp"                                 // Step table, ramp maths, tuning constants.

namespace bldc {

// Drives a three-phase inverter built from three half-bridge gate drivers of
// the IR2104 family (or any EN/IN style driver such as L6234 or DRV8313).
//
// Each phase needs two MCU signals:
//   IN  -> a TIM1 PWM channel  (decides high-side vs low-side conduction)
//   SD  -> a plain GPIO        (asserted = bridge active, deasserted = high-Z)
//
// The class owns no dynamic memory and performs no allocation at any point.
class BldcDriver {
public:
    BldcDriver() = default;                                        // Trivially constructible; real setup happens in init().

    BldcDriver(const BldcDriver&)            = delete;             // Non-copyable: it owns hardware.
    BldcDriver& operator=(const BldcDriver&) = delete;             // Non-copyable.
    BldcDriver(BldcDriver&&)                 = delete;             // Non-movable.
    BldcDriver& operator=(BldcDriver&&)      = delete;             // Non-movable.

    // Validates every device binding and forces the inverter into a safe
    // (all bridges disabled, all duties zero) state. Returns 0 on success.
    int init() noexcept;

    // Applies one entry of the six-step table at the requested duty.
    // Sequencing is float-first, then re-program, then enable, so the two
    // conducting bridges are never reconfigured while still energised.
    int apply_step(uint8_t step_index, uint32_t duty_percent) noexcept;

    // Disables all three bridges and zeroes all three PWM channels.
    // Safe to call from an error path or at any time.
    void coast() noexcept;

    // Drives the scope-trigger pin. Pulsed once per electrical revolution so a
    // scope can be triggered on a known point in the commutation sequence.
    void pulse_sync(bool level) noexcept;

private:
    int set_phase(const pwm_dt_spec& pwm_ch,
                  const gpio_dt_spec& enable_pin,
                  PhaseState state,
                  uint32_t duty_percent) noexcept;                 // Applies one phase's half of a step.

    bool ready_{false};                                            // True once init() has validated all hardware.
};

}  // namespace bldc

#endif  // BLDC_DRIVER_HPP

#pragma once

#include <array>
#include <cstdint>

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>

#include "bldc_types.hpp"

namespace bldc {

// Drives one three-phase inverter leg per Phase. Assumes each phase is
// backed by an integrated half-bridge (e.g. DRV8313 / L6234 class) that:
//   - accepts a single PWM input to set high-side duty while internally
//     handling low-side complementary switching and dead time,
//   - provides an enable/tri-state pin so the phase can be floated for
//     back-EMF sensing.
// If your hardware instead uses discrete MOSFETs with separate high/low
// gate drivers, adapt DriveHigh()/DriveLow()/Float() in phase_driver.cpp
// to drive both gate signals per phase - this class only assumes the
// 3-state (high / low / floating) contract described above.
class PhaseDriver {
public:
    struct PhaseHw {
        pwm_dt_spec pwm;      // PWM output that sets this phase's duty cycle.
        gpio_dt_spec enable;  // Active-high: enables this phase's half-bridge output.
    };

    explicit PhaseDriver(const std::array<PhaseHw, 3U>& phases);

    [[nodiscard]] bool Init();

    // Applies a commutation step: high_phase is driven with PWM at
    // duty_percent, low_phase is grounded, floating_phase is tri-stated.
    [[nodiscard]] bool ApplyStep(Phase high_phase, Phase low_phase, Phase floating_phase,
                                  uint32_t duty_percent);

    // Tri-states all three phases (coast / fault stop).
    void AllOff();

private:
    [[nodiscard]] bool DriveHigh(Phase phase, uint32_t duty_percent);
    [[nodiscard]] bool DriveLow(Phase phase);
    [[nodiscard]] bool Float(Phase phase);

    std::array<PhaseHw, 3U> phases_;
};

}  // namespace bldc

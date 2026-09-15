#pragma once

#include <cstdint>                                        // Includes fixed-size integer types.

#include <zephyr/device.h>                                // Includes Zephyr device structure and APIs.
#include <zephyr/drivers/gpio.h>                          // Includes Zephyr GPIO driver functions.


namespace motor_control {                              // Opens motor control task namespace.


// Central hardware access point. Every other module talks to the board
// through this object instead of touching devicetree macros directly.
class GpioOverlay {                                       // Creates class to manage all hardware pin and device connections.

public:

    GpioOverlay() = default;                               // Creates default constructor for GpioOverlay object.

    bool Init();                                           // Initializes GPIO pins and hardware devices.

    // ---- PWM (three-phase inverter gate-drive inputs) --------------------
    // All three phases come from TIM1, so they share one PWM device and
    // differ only by channel. A single timer also means the three phase
    // outputs stay in lockstep with each other - important when you put a
    // scope across two phases at once.

    const device* pwm_dev() const { return pwm_dev_; }      // Returns the TIM1 PWM device (drives all three phases).

    static constexpr uint32_t kPhaseUChannel = 1U;          // TIM1_CH1 -> PC0 -> phase U gate-drive input.
    static constexpr uint32_t kPhaseVChannel = 2U;          // TIM1_CH2 -> PC1 -> phase V gate-drive input.
    static constexpr uint32_t kPhaseWChannel = 3U;          // TIM1_CH3 -> PC2 -> phase W gate-drive input.

    static constexpr uint32_t kPwmPeriodNs = 50000U;        // 20 kHz PWM period (well above audible range).

    // ---- Phase ENABLE pins (driver IC enable / tri-state, one per phase) --
    // Active-HIGH in devicetree: gpio_pin_set_dt(spec, 1) == phase driven,
    // gpio_pin_set_dt(spec, 0) == phase tri-stated (floating).

    const gpio_dt_spec& en_u() const { return en_u_; }      // Returns phase U enable pin spec (PB0).
    const gpio_dt_spec& en_v() const { return en_v_; }      // Returns phase V enable pin spec (PB1).
    const gpio_dt_spec& en_w() const { return en_w_; }      // Returns phase W enable pin spec (PB2).

    // ---- Scope sync outputs (waveform observation aids) -------------------
    // These carry no motor current. They exist purely so the commutation
    // sequence can be observed on a scope alongside the phase waveforms.

    const gpio_dt_spec& step_sync() const { return step_sync_; }    // Returns per-step sync pin spec (PB4).
    const gpio_dt_spec& cycle_sync() const { return cycle_sync_; }  // Returns per-electrical-cycle sync pin spec (PB5).


private:

    const device* pwm_dev_ = nullptr;                       // Stores TIM1 PWM device pointer (all three phases).

    gpio_dt_spec en_u_{};                                   // Stores phase U enable pin spec.
    gpio_dt_spec en_v_{};                                   // Stores phase V enable pin spec.
    gpio_dt_spec en_w_{};                                   // Stores phase W enable pin spec.

    gpio_dt_spec step_sync_{};                              // Stores per-commutation-step scope sync pin spec.
    gpio_dt_spec cycle_sync_{};                             // Stores per-electrical-cycle scope trigger pin spec.

    bool initialized_ = false;                              // Stores whether hardware initialization is completed.
};


}  // namespace motor_control

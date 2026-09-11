#pragma once

#include <cstdint>                                        // Includes fixed-size integer types.

#include <zephyr/device.h>                                // Includes Zephyr device structure and APIs.
#include <zephyr/drivers/gpio.h>                          // Includes Zephyr GPIO driver functions.


namespace motor_control {                            // Opens motor control task namespace.


// Central hardware access point for the current-control task. Only what a
// single-phase/coil closed-loop current limiter needs: one PWM leg, one
// enable (SD) pin, and one current-sense ADC channel. No LCD, no voltage
// sensing, no second bridge leg, no button - the loop free-runs from boot.
class GpioOverlay {                                       // Creates class to manage all hardware pin and device connections.

public:

    GpioOverlay() = default;                               // Creates default constructor for GpioOverlay object.

    bool Init();                                           // Initializes GPIO pins and hardware devices.

    // ---- PWM (single IR2104 gate-drive PWM input, pin 2) ----------------

    const device* pwm_dev() const { return pwm_dev_; }      // Returns the TIM1 PWM device driving the coil leg.

    static constexpr uint32_t kPwmChannel = 1U;              // TIM1_CH1 -> PC0 -> IR2104 PWM input.
    static constexpr uint32_t kPwmPeriodNs = 50000U;          // 20 kHz PWM period (well above audible range).

    // ---- IR2104 SHUTDOWN / enable pin (pin 3) ----------------------------
    // Active-LOW in devicetree: gpio_pin_set_dt(spec, 1) == enabled (SD low),
    // gpio_pin_set_dt(spec, 0) == shutdown (SD high), matching IR2104 datasheet.

    const gpio_dt_spec& enable_pin() const { return enable_pin_; }  // Returns the coil-driver enable/shutdown pin spec (PC1).

    // ---- ADC (coil current sensing, ACS712) --------------------------------

    const device* current_adc_dev() const { return current_adc_dev_; } // Returns ADC1 device (ACS712 current sensor).

    static constexpr uint8_t kCurrentAdcChannel = 1U;         // ADC1_IN1 -> PA0 -> ACS712 output.
    static constexpr uint8_t kAdcResolutionBits = 12U;        // STM32G4 ADC resolution.


private:

    const device* pwm_dev_ = nullptr;                        // Stores TIM1 PWM device pointer.

    gpio_dt_spec enable_pin_{};                               // Stores IR2104 shutdown/enable pin spec.

    const device* current_adc_dev_ = nullptr;                  // Stores ADC1 device pointer (current sensor).

    bool initialized_ = false;                                  // Stores whether hardware initialization is completed.
};


}  // namespace motor_control

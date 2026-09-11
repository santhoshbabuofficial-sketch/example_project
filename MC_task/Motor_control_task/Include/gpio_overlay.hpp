#pragma once                                              // Prevents this header file from being included multiple times.

#include <cstdint>                                        // Includes fixed-size integer types.

#include <zephyr/device.h>                                // Includes Zephyr device structure and APIs.
#include <zephyr/drivers/gpio.h>                          // Includes Zephyr GPIO driver functions.


namespace mc::motor_control {                            // Opens motor control task namespace.


// Central hardware access point. Every other module talks to the board
// only through this class - no raw devicetree macros anywhere else.
class GpioOverlay {                                       // Creates class to manage all hardware pin and device connections.

public:

    GpioOverlay() = default;                               // Creates default constructor for GpioOverlay object.

    bool Init();                                           // Initializes GPIO pins and hardware devices.

    // ---- PWM (IR2104 gate-drive PWM inputs, pin 2 of each IC) ----------
    // Both legs live on the same TIM1 peripheral (different channels), so
    // there is a single PWM device shared by both - not two separate ones.

    const device* pwm_dev() const { return pwm_dev_; }      // Returns the TIM1 PWM device (drives both IR2104 legs).

    static constexpr uint32_t kPwm1Channel = 1U;            // TIM1_CH1 -> PC0 -> IR2104 #1 PWM input.
    static constexpr uint32_t kPwm2Channel = 3U;            // TIM1_CH3 -> PC2 -> IR2104 #2 PWM input.
    static constexpr uint32_t kPwmPeriodNs = 50000U;        // 20 kHz PWM period (well above audible range).

    // ---- IR2104 SHUTDOWN pins (pin 3 of each IC) ------------------------
    // Active-LOW in devicetree: gpio_pin_set_dt(spec, 1) == enabled (SD low),
    // gpio_pin_set_dt(spec, 0) == shutdown (SD high), matching IR2104 datasheet.

    const gpio_dt_spec& sd1() const { return sd1_; }        // Returns IR2104 #1 shutdown pin spec (PC1).
    const gpio_dt_spec& sd2() const { return sd2_; }        // Returns IR2104 #2 shutdown pin spec (PC3).

    // ---- Motor direction indicator pins ---------------------------------

    const gpio_dt_spec& dir_fwd() const { return dir_fwd_; } // Returns forward-direction indicator pin (PB13).
    const gpio_dt_spec& dir_rev() const { return dir_rev_; } // Returns reverse-direction indicator pin (PB15).

    // ---- User button (onboard B1) ---------------------------------------

    const gpio_dt_spec& user_button() const { return user_button_; } // Returns onboard user button pin spec.

    // ---- ADC (current / voltage sensing) ---------------------------------

    const device* current_adc_dev() const { return current_adc_dev_; } // Returns ADC1 device (ACS712 current sensor).
    const device* voltage_adc_dev() const { return voltage_adc_dev_; } // Returns ADC2 device (0-25V voltage sensor).

    static constexpr uint8_t kCurrentAdcChannel = 1U;        // ADC1_IN1 -> PA0 -> ACS712 output.
    static constexpr uint8_t kVoltageAdcChannel = 17U;       // ADC2_IN17 -> PA4 -> voltage divider output.
    static constexpr uint8_t kAdcResolutionBits = 12U;       // STM32G4 ADC resolution used for both channels.

    // ---- LCD (16x2, PCF8574 I2C backpack) --------------------------------

    const device* lcd_i2c_bus() const { return lcd_i2c_bus_; } // Returns I2C device used for LCD communication.

    static constexpr uint16_t kLcdI2cAddr = 0x27U;            // I2C address of LCD PCF8574 backpack.


private:

    const device* pwm_dev_ = nullptr;                       // Stores TIM1 PWM device pointer (drives both legs).

    gpio_dt_spec sd1_{};                                    // Stores IR2104 #1 shutdown pin spec.
    gpio_dt_spec sd2_{};                                    // Stores IR2104 #2 shutdown pin spec.

    gpio_dt_spec dir_fwd_{};                                 // Stores forward direction indicator pin spec.
    gpio_dt_spec dir_rev_{};                                 // Stores reverse direction indicator pin spec.

    gpio_dt_spec user_button_{};                             // Stores onboard user button pin spec.

    const device* current_adc_dev_ = nullptr;                // Stores ADC1 device pointer (current sensor).
    const device* voltage_adc_dev_ = nullptr;                // Stores ADC2 device pointer (voltage sensor).

    const device* lcd_i2c_bus_ = nullptr;                     // Stores I2C device pointer used for LCD.

    bool initialized_ = false;                                // Stores whether hardware initialization is completed.
};


}  // namespace mc::motor_control                          // Closes motor control namespace.

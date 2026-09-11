#include "gpio_overlay.hpp"                                       // Includes GpioOverlay class definition.

#include <zephyr/devicetree.h>                                    // Includes Zephyr device tree support.
#include <zephyr/drivers/adc.h>                                   // Includes Zephyr ADC driver functions.
#include <zephyr/drivers/gpio.h>                                  // Includes Zephyr GPIO control functions.
#include <zephyr/logging/log.h>                                   // Includes Zephyr logging functions.


LOG_MODULE_REGISTER(current_ctrl_gpio_overlay, LOG_LEVEL_INF);    // Creates log module for GPIO overlay.


namespace motor_control {                                    // Opens motor control task namespace.


bool GpioOverlay::Init() {                                        // Initializes all hardware devices and pins.

    bool all_ready = true;                                        // Tracks whether every device/pin initialized cleanly.

    // ---- PWM device (TIM1, single coil leg) -------------------------------

    pwm_dev_ = DEVICE_DT_GET(DT_ALIAS(coilpwm));                  // Gets the TIM1 PWM device for the coil leg.

    if (!device_is_ready(pwm_dev_)) {
        LOG_ERR("TIM1 PWM device not ready");                      // Prints PWM device error.
        all_ready = false;                                        // Marks initialization as failed.
    }

    // ---- IR2104 SHUTDOWN / enable pin --------------------------------------

    enable_pin_ = GPIO_DT_SPEC_GET(DT_ALIAS(coilen), gpios);       // Resolves the coil-driver enable/shutdown pin spec.

    if (!gpio_is_ready_dt(&enable_pin_)) {
        LOG_ERR("Coil driver enable pin not ready");               // Prints enable pin error.
        all_ready = false;                                        // Marks initialization as failed.
    } else {
        // Start disabled (SD high / driver shut down) until the current
        // loop explicitly enables it - safest power-up state.
        gpio_pin_configure_dt(&enable_pin_, GPIO_OUTPUT_INACTIVE); // Configures enable pin as output, starts disabled.
    }

    // ---- ADC device (coil current sensing) -----------------------------------

    current_adc_dev_ = DEVICE_DT_GET(DT_ALIAS(currentsensor));    // Gets ADC1 device (ACS712 current sensor).

    if (!device_is_ready(current_adc_dev_)) {
        LOG_ERR("Current sensor ADC device not ready");            // Prints ADC device error.
        all_ready = false;                                        // Marks initialization as failed.
    } else {

        const adc_channel_cfg current_cfg = {
            .gain = ADC_GAIN_1,                                    // Uses unity gain (no internal amplification).
            .reference = ADC_REF_INTERNAL,                         // Uses internal ADC reference voltage.
            .acquisition_time = ADC_ACQ_TIME_DEFAULT,               // Uses driver default acquisition time.
            .channel_id = kCurrentAdcChannel,                       // Selects ADC1_IN1 (PA0).
            .differential = 0,                                      // Uses single-ended measurement.
        };

        if (adc_channel_setup(current_adc_dev_, &current_cfg) != 0) {
            LOG_ERR("Current sensor ADC channel setup failed");    // Prints ADC channel configuration error.
            all_ready = false;                                      // Marks initialization as failed.
        }
    }

    if (!all_ready) {                                                // Checks if any hardware initialization failed.
        return false;                                                // Stops initialization and returns failure.
    }

    initialized_ = true;                                             // Stores that hardware initialization is completed.

    return true;                                                     // Returns successful initialization.
}


}  // namespace motor_control

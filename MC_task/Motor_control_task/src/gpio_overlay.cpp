#include "gpio_overlay.hpp"                                       // Includes GpioOverlay class definition.

#include <zephyr/devicetree.h>                                    // Includes Zephyr device tree support.
#include <zephyr/drivers/adc.h>                                   // Includes Zephyr ADC driver functions.
#include <zephyr/drivers/gpio.h>                                  // Includes Zephyr GPIO control functions.
#include <zephyr/logging/log.h>                                   // Includes Zephyr logging functions.


LOG_MODULE_REGISTER(motor_control_gpio_overlay, LOG_LEVEL_INF);   // Creates log module for GPIO overlay.


namespace mc::motor_control {                                    // Opens motor control task namespace.


bool GpioOverlay::Init() {                                        // Initializes all hardware devices and pins.

    bool all_ready = true;                                        // Tracks whether every device/pin initialized cleanly.

    // ---- PWM device (TIM1, shared by both IR2104 legs) -------------------

    pwm_dev_ = DEVICE_DT_GET(DT_ALIAS(ir2104pwm));                // Gets the TIM1 PWM device (both legs, different channels).

    if (!device_is_ready(pwm_dev_)) {
        LOG_ERR("TIM1 PWM device not ready");                      // Prints PWM device error.
        all_ready = false;                                        // Marks initialization as failed.
    }

    // ---- IR2104 SHUTDOWN pins -------------------------------------------

    sd1_ = GPIO_DT_SPEC_GET(DT_ALIAS(sd1), gpios);                // Resolves IR2104 #1 shutdown pin spec.
    sd2_ = GPIO_DT_SPEC_GET(DT_ALIAS(sd2), gpios);                // Resolves IR2104 #2 shutdown pin spec.

    if (!gpio_is_ready_dt(&sd1_) || !gpio_is_ready_dt(&sd2_)) {
        LOG_ERR("IR2104 SD pin(s) not ready");                    // Prints SD pin error.
        all_ready = false;                                        // Marks initialization as failed.
    } else {
        // Start with both half-bridges disabled (SD high) until the motor
        // control state machine explicitly enables them - safest power-up state.
        gpio_pin_configure_dt(&sd1_, GPIO_OUTPUT_INACTIVE);       // Configures SD1 as output, starts disabled.
        gpio_pin_configure_dt(&sd2_, GPIO_OUTPUT_INACTIVE);       // Configures SD2 as output, starts disabled.
    }

    // ---- Direction indicator pins ---------------------------------------

    dir_fwd_ = GPIO_DT_SPEC_GET(DT_ALIAS(dirfwd), gpios);         // Resolves forward direction indicator pin.
    dir_rev_ = GPIO_DT_SPEC_GET(DT_ALIAS(dirrev), gpios);         // Resolves reverse direction indicator pin.

    if (!gpio_is_ready_dt(&dir_fwd_) || !gpio_is_ready_dt(&dir_rev_)) {
        LOG_ERR("Direction indicator pin(s) not ready");          // Prints direction pin error.
        all_ready = false;                                        // Marks initialization as failed.
    } else {
        gpio_pin_configure_dt(&dir_fwd_, GPIO_OUTPUT_INACTIVE);   // Configures FWD indicator, starts LOW.
        gpio_pin_configure_dt(&dir_rev_, GPIO_OUTPUT_INACTIVE);   // Configures REV indicator, starts LOW.
    }

    // ---- User button ------------------------------------------------------

    user_button_ = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);        // Resolves onboard user button pin spec.

    if (!gpio_is_ready_dt(&user_button_)) {
        LOG_ERR("User button not ready");                          // Prints button error.
        all_ready = false;                                        // Marks initialization as failed.
    } else {
        gpio_pin_configure_dt(&user_button_, GPIO_INPUT);         // Configures user button as input.
    }

    // ---- ADC devices (current / voltage sensing) ---------------------------

    current_adc_dev_ = DEVICE_DT_GET(DT_ALIAS(currentsensor));    // Gets ADC1 device (ACS712 current sensor).
    voltage_adc_dev_ = DEVICE_DT_GET(DT_ALIAS(voltagesensor));    // Gets ADC2 device (voltage sensor module).

    if (!device_is_ready(current_adc_dev_) || !device_is_ready(voltage_adc_dev_)) {
        LOG_ERR("ADC device(s) not ready");                        // Prints ADC device error.
        all_ready = false;                                        // Marks initialization as failed.
    } else {

        const adc_channel_cfg current_cfg = {
            .gain = ADC_GAIN_1,                                    // Uses unity gain (no internal amplification).
            .reference = ADC_REF_INTERNAL,                         // Uses internal ADC reference voltage.
            .acquisition_time = ADC_ACQ_TIME_DEFAULT,               // Uses driver default acquisition time.
            .channel_id = kCurrentAdcChannel,                       // Selects ADC1_IN1 (PA0).
            .differential = 0,                                      // Uses single-ended measurement.
        };

        const adc_channel_cfg voltage_cfg = {
            .gain = ADC_GAIN_1,                                    // Uses unity gain.
            .reference = ADC_REF_INTERNAL,                         // Uses internal ADC reference voltage.
            .acquisition_time = ADC_ACQ_TIME_DEFAULT,               // Uses driver default acquisition time.
            .channel_id = kVoltageAdcChannel,                       // Selects ADC2_IN17 (PA4).
            .differential = 0,                                      // Uses single-ended measurement.
        };

        if (adc_channel_setup(current_adc_dev_, &current_cfg) != 0 ||
            adc_channel_setup(voltage_adc_dev_, &voltage_cfg) != 0) {
            LOG_ERR("ADC channel setup failed");                    // Prints ADC channel configuration error.
            all_ready = false;                                      // Marks initialization as failed.
        }
    }

    // ---- LCD I2C bus --------------------------------------------------------

    lcd_i2c_bus_ = DEVICE_DT_GET(DT_BUS(DT_ALIAS(lcddisplay)));    // Gets I2C device connected to LCD.

    if (!device_is_ready(lcd_i2c_bus_)) {
        LOG_ERR("LCD I2C bus not ready");                           // Prints LCD I2C error.
        all_ready = false;                                          // Marks initialization as failed.
    }

    if (!all_ready) {                                                // Checks if any hardware initialization failed.
        return false;                                                // Stops initialization and returns failure.
    }

    initialized_ = true;                                             // Stores that hardware initialization is completed.

    return true;                                                     // Returns successful initialization.
}


}  // namespace mc::motor_control                                  // Closes motor control namespace.

#include "gpio_overlay.hpp"                                       // Includes GpioOverlay class definition.

#include <zephyr/devicetree.h>                                    // Includes Zephyr device tree support.
#include <zephyr/drivers/gpio.h>                                  // Includes Zephyr GPIO control functions.
#include <zephyr/drivers/pwm.h>                                   // Includes Zephyr PWM driver functions.
#include <zephyr/logging/log.h>                                   // Includes Zephyr logging functions.


LOG_MODULE_REGISTER(bldc_gpio_overlay, LOG_LEVEL_INF);            // Creates log module for GPIO overlay.


namespace motor_control {                                    // Opens motor control task namespace.


bool GpioOverlay::Init() {                                        // Initializes all hardware devices and pins.

    bool all_ready = true;                                        // Tracks whether every device/pin initialized cleanly.

    // ---- PWM device (TIM1, shared by all three phases) -------------------

    pwm_dev_ = DEVICE_DT_GET(DT_ALIAS(bldcpwm));                  // Gets the TIM1 PWM device (all phases, different channels).

    if (!device_is_ready(pwm_dev_)) {
        LOG_ERR("TIM1 PWM device not ready");                      // Prints PWM device error.
        all_ready = false;                                        // Marks initialization as failed.
    }

    // ---- Phase enable pins -----------------------------------------------

    en_u_ = GPIO_DT_SPEC_GET(DT_ALIAS(enu), gpios);               // Resolves phase U enable pin spec.
    en_v_ = GPIO_DT_SPEC_GET(DT_ALIAS(env), gpios);               // Resolves phase V enable pin spec.
    en_w_ = GPIO_DT_SPEC_GET(DT_ALIAS(enw), gpios);               // Resolves phase W enable pin spec.

    if (!gpio_is_ready_dt(&en_u_) || !gpio_is_ready_dt(&en_v_) || !gpio_is_ready_dt(&en_w_)) {
        LOG_ERR("Phase enable pin(s) not ready");                  // Prints enable pin error.
        all_ready = false;                                         // Marks initialization as failed.
    } else {
        // All three phases start tri-stated. The motor is electrically
        // disconnected until the commutation loop explicitly drives it -
        // safest power-up state.
        gpio_pin_configure_dt(&en_u_, GPIO_OUTPUT_INACTIVE);       // Configures phase U enable, starts floating.
        gpio_pin_configure_dt(&en_v_, GPIO_OUTPUT_INACTIVE);       // Configures phase V enable, starts floating.
        gpio_pin_configure_dt(&en_w_, GPIO_OUTPUT_INACTIVE);       // Configures phase W enable, starts floating.
    }

    // ---- Scope sync pins ---------------------------------------------------

    step_sync_  = GPIO_DT_SPEC_GET(DT_ALIAS(stepsync), gpios);    // Resolves per-commutation-step sync pin.
    cycle_sync_ = GPIO_DT_SPEC_GET(DT_ALIAS(cyclesync), gpios);   // Resolves per-electrical-cycle sync pin.

    if (!gpio_is_ready_dt(&step_sync_) || !gpio_is_ready_dt(&cycle_sync_)) {
        LOG_ERR("Scope sync pin(s) not ready");                    // Prints sync pin error.
        all_ready = false;                                         // Marks initialization as failed.
    } else {
        gpio_pin_configure_dt(&step_sync_, GPIO_OUTPUT_INACTIVE);  // Configures step sync output, starts LOW.
        gpio_pin_configure_dt(&cycle_sync_, GPIO_OUTPUT_INACTIVE); // Configures cycle sync output, starts LOW.
    }

    if (!all_ready) {                                               // Checks if any hardware initialization failed.
        return false;                                               // Stops initialization and returns failure.
    }

    initialized_ = true;                                            // Stores that hardware initialization is completed.

    return true;                                                    // Returns successful initialization.
}


}  // namespace motor_control

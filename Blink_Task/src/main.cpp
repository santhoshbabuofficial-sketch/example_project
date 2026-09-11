#include <zephyr/kernel.h>                                  
#include <zephyr/logging/log.h>                                  
#include <zephyr/drivers/gpio.h>                                


LOG_MODULE_REGISTER(blink_task_main, LOG_LEVEL_INF);           


namespace {                                                       


constexpr uint16_t kBlinkPeriodMs = 500U;                         

const gpio_dt_spec g_led = GPIO_DT_SPEC_GET(DT_ALIAS(blinkled), gpios);


} 


int main() {                                                      

    LOG_INF("blink task booting");                                

    if (!gpio_is_ready_dt(&g_led)) {                               

        LOG_ERR("led gpio device not ready - halting");                 

        return -1;                                                       
    }

    const int config_result =
        gpio_pin_configure_dt(&g_led, GPIO_OUTPUT_INACTIVE);

    if (config_result != 0) {                                       

        LOG_ERR("led gpio configure failed (%d) - halting", config_result);
        // Prints configuration error along with the returned error code.

        return -1;                                                       // Stops program on failure.
    }

    LOG_INF("blink task running");                                      // Prints running message.

    while (true) {                                                       // Creates infinite RTOS task loop.

        gpio_pin_toggle_dt(&g_led);                                        // Toggles the LED output state.

        k_msleep(kBlinkPeriodMs);                                          // Delays thread for the blink period.
    }

    return 0;                                                             // Unreachable; kept for a well-defined return path.
}

#include <zephyr/kernel.h>                                        // Includes Zephyr RTOS kernel functions like sleep.
#include <zephyr/device.h>                                        // Includes Zephyr device structure and APIs.
#include <zephyr/devicetree.h>                                    // Includes Zephyr device tree support.
#include <zephyr/drivers/pwm.h>                                   // Includes Zephyr PWM control functions.
#include <zephyr/logging/log.h>                                   // Includes Zephyr logging functions.


LOG_MODULE_REGISTER(pwm_50_percent_task, LOG_LEVEL_INF);          // Creates log module for this PWM task.


namespace {                                                        // Opens private namespace for local constants.


constexpr uint32_t kPwmPeriodNs = 50000U;                          // 20 kHz PWM period (1 / 20000 Hz = 50000 ns).
constexpr uint32_t kPwmDutyPercent = 50U;                          // Desired fixed duty cycle (50%).
constexpr uint32_t kPwmChannel = 1U;                               // TIM1_CH1 -> PC0 output channel.


}  // namespace                                                    // Closes private namespace.


int main() {                                                        // Main function starts the PWM task.

    LOG_INF("pwm 50%% @ 20kHz task booting");                        // Prints startup message.

    const device* pwm_dev = DEVICE_DT_GET(DT_ALIAS(pwmtest));          // Gets the TIM1 PWM device from the board overlay.

    if (!device_is_ready(pwm_dev)) {                                    // Checks that the PWM device initialized correctly.

        LOG_ERR("TIM1 PWM device not ready - halting");                   // Prints PWM device error.

        return -1;                                                          // Stops program due to hardware failure.
    }

    const uint32_t pulse_ns = (kPwmPeriodNs * kPwmDutyPercent) / 100U;       // Converts 50% duty into a pulse width in nanoseconds.

    const int err =
        pwm_set(pwm_dev, kPwmChannel, kPwmPeriodNs, pulse_ns, PWM_POLARITY_NORMAL);
    // Applies 20 kHz period / 50% duty cycle to TIM1_CH1 (PC0).

    if (err != 0) {                                                         // Checks whether the PWM call succeeded.

        LOG_ERR("pwm_set failed (err=%d) - halting", err);                     // Prints PWM configuration error.

        return -1;                                                             // Stops program on failure.
    }

    LOG_INF("pwm running: 50%% duty @ 20kHz on TIM1_CH1 (PC0)");                // Confirms PWM is now running.

    while (true) {                                                            // Keeps the thread alive; PWM output is hardware-driven.

        k_msleep(1000);                                                        // Sleeps 1 second per idle loop iteration.
    }

    return 0;                                                                  // Unreachable, but ends main function cleanly.
}

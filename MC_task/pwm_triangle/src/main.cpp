#include <zephyr/kernel.h>                                        // Includes Zephyr RTOS kernel functions like sleep.
#include <zephyr/device.h>                                        // Includes Zephyr device structure and APIs.
#include <zephyr/devicetree.h>                                    // Includes Zephyr device tree support.
#include <zephyr/drivers/pwm.h>                                   // Includes Zephyr PWM control functions.
#include <zephyr/logging/log.h>                                   // Includes Zephyr logging functions.


LOG_MODULE_REGISTER(pwm_triangle_wave_task, LOG_LEVEL_INF);       // Creates log module for this PWM task.


namespace {                                                        // Opens private namespace for local constants.


constexpr uint32_t kPwmPeriodNs = 50000U;                          // 20 kHz PWM carrier period (1 / 20000 Hz = 50000 ns).
constexpr uint32_t kPwmChannel = 1U;                               // TIM1_CH1 -> PC0 output channel.

// A hardware PWM peripheral can only switch fully on/off each cycle, so a
// "triangular wave" is produced by sweeping the duty cycle up and down
// between a min and max value every step. Feed this output through an
// external RC low-pass filter to recover a smooth triangular voltage.
constexpr uint32_t kDutyMinPercent = 0U;                          // Duty cycle at the bottom of the triangle.
constexpr uint32_t kDutyMaxPercent = 100U;                        // Duty cycle at the top of the triangle.
constexpr uint32_t kDutyStepPercent = 1U;                         // Duty cycle change applied each step.
constexpr uint32_t kStepDelayMs = 5U;                             // Delay between steps -> sets the triangle wave frequency.


}  // namespace                                                    // Closes private namespace.


int main() {                                                        // Main function starts the PWM task.

    LOG_INF("pwm triangular wave @ 20kHz carrier task booting");     // Prints startup message.

    const device* pwm_dev = DEVICE_DT_GET(DT_ALIAS(pwmtest));          // Gets the TIM1 PWM device from the board overlay.

    if (!device_is_ready(pwm_dev)) {                                    // Checks that the PWM device initialized correctly.

        LOG_ERR("TIM1 PWM device not ready - halting");                   // Prints PWM device error.

        return -1;                                                          // Stops program due to hardware failure.
    }

    uint32_t duty_percent = kDutyMinPercent;                                // Current duty cycle, starts at the bottom of the ramp.
    bool rising = true;                                                    // Tracks ramp direction (up or down).

    while (true) {                                                         // Keeps the thread alive, continuously sweeping duty cycle.

        const uint32_t pulse_ns = (kPwmPeriodNs * duty_percent) / 100U;      // Converts current duty % into a pulse width in nanoseconds.

        const int err =
            pwm_set(pwm_dev, kPwmChannel, kPwmPeriodNs, pulse_ns, PWM_POLARITY_NORMAL);
        // Applies 20 kHz period / current duty cycle to TIM1_CH1 (PC0).

        if (err != 0) {                                                      // Checks whether the PWM call succeeded.

            LOG_ERR("pwm_set failed (err=%d) - halting", err);                  // Prints PWM configuration error.

            return -1;                                                          // Stops program on failure.
        }

        if (rising) {                                                        // Ramping duty cycle upward.

            if (duty_percent >= kDutyMaxPercent) {                             // Reached the top of the triangle.

                duty_percent = kDutyMaxPercent;                                  // Clamp to the max value.
                rising = false;                                                  // Switch direction to falling.
            } else {

                duty_percent += kDutyStepPercent;                                // Step duty cycle up.
            }

        } else {                                                              // Ramping duty cycle downward.

            if (duty_percent <= kDutyMinPercent) {                             // Reached the bottom of the triangle.

                duty_percent = kDutyMinPercent;                                  // Clamp to the min value.
                rising = true;                                                   // Switch direction to rising.
            } else {

                duty_percent -= kDutyStepPercent;                                // Step duty cycle down.
            }
        }

        k_msleep(kStepDelayMs);                                              // Waits before the next duty cycle step.
    }

    return 0;                                                                  // Unreachable, but ends main function cleanly.
}

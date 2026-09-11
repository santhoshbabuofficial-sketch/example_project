#include <zephyr/kernel.h>                                        // Includes Zephyr RTOS kernel functions like sleep.
#include <zephyr/device.h>                                        // Includes Zephyr device structure and APIs.
#include <zephyr/devicetree.h>                                    // Includes Zephyr device tree support.
#include <zephyr/drivers/pwm.h>                                   // Includes Zephyr PWM control functions.
#include <zephyr/logging/log.h>                                   // Includes Zephyr logging functions.


LOG_MODULE_REGISTER(pwm_sine_wave_task, LOG_LEVEL_INF);           // Creates log module for this PWM task.


namespace {                                                        // Opens private namespace for local constants.


constexpr uint32_t kPwmPeriodNs = 50000U;                          // 20 kHz PWM carrier period (1 / 20000 Hz = 50000 ns).
constexpr uint32_t kPwmChannel = 1U;                               // TIM1_CH1 -> PC0 output channel.

// A hardware PWM peripheral can only switch fully on/off each cycle, so a
// "sine wave" is produced by stepping the duty cycle through a precomputed
// sine lookup table (0-100%, centered on 50%). Feed this output through an
// external RC low-pass filter to recover a smooth sinusoidal voltage.
constexpr uint32_t kSineTableSize = 100U;                          // Number of duty-cycle samples per full sine cycle.
constexpr uint32_t kStepDelayMs = 2U;                              // Delay between samples -> sets the sine wave frequency.

// Precomputed duty cycle (%) for one full sine cycle: 50 + 50*sin(2*pi*i/N).
constexpr uint32_t kSineDutyTable[kSineTableSize] = {
    50, 53, 56, 59, 62, 65, 68, 71, 74, 77,
    79, 82, 84, 86, 89, 90, 92, 94, 95, 96,
    98, 98, 99, 100, 100, 100, 100, 100, 99, 98,
    98, 96, 95, 94, 92, 90, 89, 86, 84, 82,
    79, 77, 74, 71, 68, 65, 62, 59, 56, 53,
    50, 47, 44, 41, 38, 35, 32, 29, 26, 23,
    21, 18, 16, 14, 11, 10, 8, 6, 5, 4,
    2, 2, 1, 0, 0, 0, 0, 0, 1, 2,
    2, 4, 5, 6, 8, 10, 11, 14, 16, 18,
    21, 23, 26, 29, 32, 35, 38, 41, 44, 47,
};


}  // namespace                                                    // Closes private namespace.


int main() {                                                        // Main function starts the PWM task.

    LOG_INF("pwm sine wave @ 20kHz carrier task booting");           // Prints startup message.

    const device* pwm_dev = DEVICE_DT_GET(DT_ALIAS(pwmtest));          // Gets the TIM1 PWM device from the board overlay.

    if (!device_is_ready(pwm_dev)) {                                    // Checks that the PWM device initialized correctly.

        LOG_ERR("TIM1 PWM device not ready - halting");                   // Prints PWM device error.

        return -1;                                                          // Stops program due to hardware failure.
    }

    uint32_t table_index = 0U;                                              // Current position in the sine lookup table.

    while (true) {                                                         // Keeps the thread alive, continuously stepping through the table.

        const uint32_t duty_percent = kSineDutyTable[table_index];           // Reads the current sample's duty cycle (%).
        const uint32_t pulse_ns = (kPwmPeriodNs * duty_percent) / 100U;      // Converts current duty % into a pulse width in nanoseconds.

        const int err =
            pwm_set(pwm_dev, kPwmChannel, kPwmPeriodNs, pulse_ns, PWM_POLARITY_NORMAL);
        // Applies 20 kHz period / current duty cycle to TIM1_CH1 (PC0).

        if (err != 0) {                                                      // Checks whether the PWM call succeeded.

            LOG_ERR("pwm_set failed (err=%d) - halting", err);                  // Prints PWM configuration error.

            return -1;                                                          // Stops program on failure.
        }

        table_index = (table_index + 1U) % kSineTableSize;                   // Advances to the next sample, wrapping at the table size.

        k_msleep(kStepDelayMs);                                              // Waits before the next sample step.
    }

    return 0;                                                                  // Unreachable, but ends main function cleanly.
}

#include <zephyr/kernel.h>                                        // Includes Zephyr RTOS kernel functions like sleep.
#include <zephyr/device.h>                                        // Includes Zephyr device structure and APIs.
#include <zephyr/devicetree.h>                                    // Includes Zephyr device tree support.
#include <zephyr/drivers/pwm.h>                                   // Includes Zephyr PWM control functions.
#include <zephyr/logging/log.h>                                   // Includes Zephyr logging functions.


LOG_MODULE_REGISTER(pwm_ramp_task, LOG_LEVEL_INF);                 // Creates log module for this PWM ramp task.


namespace {                                                        // Opens private namespace for local constants.


constexpr uint32_t kPwmPeriodNs = 50000U;                          // 20 kHz PWM period (1 / 20000 Hz = 50000 ns).
constexpr uint32_t kPwmChannel = 1U;                               // TIM1_CH1 -> PC0 output channel.

constexpr uint16_t kRampStepPeriodMs = 400U;                       // Defines how often the PWM ramp advances by one 5% step.

static constexpr uint8_t kPwmStepPercent = 1U;                     // PWM ramp step size, per task spec (1% steps).
static constexpr uint8_t kPwmMaxPercent = 80U;                     // Maximum PWM duty percentage.
static constexpr uint8_t kPwmMinPercent = 0U;                      // Minimum PWM duty percentage.


}  // namespace                                                    // Closes private namespace.


int main() {                                                        // Main function starts the PWM ramp task.

    LOG_INF("pwm ramp @ 20kHz task booting");                        // Prints startup message.

    const device* pwm_dev = DEVICE_DT_GET(DT_ALIAS(pwmtest));          // Gets the TIM1 PWM device from the board overlay.

    if (!device_is_ready(pwm_dev)) {                                    // Checks that the PWM device initialized correctly.

        LOG_ERR("TIM1 PWM device not ready - halting");                   // Prints PWM device error.

        return -1;                                                          // Stops program due to hardware failure.
    }

    uint8_t duty_percent = kPwmMinPercent;                                  // Tracks current ramp duty, starting at minimum.

    while (duty_percent <= kPwmMaxPercent) {                                // Ramps duty upward one step at a time.

        const uint32_t pulse_ns = (kPwmPeriodNs * duty_percent) / 100U;        // Converts current duty percent into a pulse width in nanoseconds.

        const int err =
            pwm_set(pwm_dev, kPwmChannel, kPwmPeriodNs, pulse_ns, PWM_POLARITY_NORMAL);
        // Applies 20 kHz period / current duty cycle to TIM1_CH1 (PC0).

        if (err != 0) {                                                         // Checks whether the PWM call succeeded.

            LOG_ERR("pwm_set failed (err=%d) - halting", err);                     // Prints PWM configuration error.

            return -1;                                                             // Stops program on failure.
        }

        LOG_INF("pwm ramp: %u%% duty @ 20kHz on TIM1_CH1 (PC0)", duty_percent);       // Confirms current ramp step is applied.

        if (duty_percent == kPwmMaxPercent) {                                       // Checks if ramp has reached maximum duty.

            break;                                                                     // Stops the ramp loop at max duty.
        }

        duty_percent += kPwmStepPercent;                                            // Advances duty by one step for the next iteration.

        k_msleep(kRampStepPeriodMs);                                                // Waits before applying the next ramp step.
    }

    while (true) {                                                            // Holds output at max duty; PWM is hardware-driven.

        LOG_INF("pwm holding: %u%% duty @ 20kHz on TIM1_CH1 (PC0)", kPwmMaxPercent);  // Confirms ramp target is being held.

        k_msleep(1000);                                                        // Sleeps 1 second per idle loop iteration.
    }

    return 0;                                                                  // Unreachable, but ends main function cleanly.
}

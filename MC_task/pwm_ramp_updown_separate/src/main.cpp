#include <zephyr/kernel.h>                                        // Includes Zephyr RTOS kernel functions like sleep.
#include <zephyr/device.h>                                        // Includes Zephyr device structure and APIs.
#include <zephyr/devicetree.h>                                    // Includes Zephyr device tree support.
#include <zephyr/drivers/pwm.h>                                   // Includes Zephyr PWM control functions.
#include <zephyr/logging/log.h>                                   // Includes Zephyr logging functions.


LOG_MODULE_REGISTER(pwm_ramp_task, LOG_LEVEL_INF);                 // Creates log module for this PWM ramp task.


namespace {                                                        // Opens private namespace for local constants.


constexpr uint32_t kPwmPeriodNs = 50000U;                          // 20 kHz PWM period (1 / 20000 Hz = 50000 ns).
constexpr uint32_t kPwmChannel = 1U;                               // TIM1_CH1 -> PC0 output channel.

constexpr uint16_t kRampUpStepPeriodMs = 400U;                     // Defines how often the PWM ramp-up advances by one step.
constexpr uint16_t kRampDownStepPeriodMs = 400U;                   // Defines how often the PWM ramp-down advances by one step.
constexpr uint16_t kRampHoldPeriodMs = 10000U;                     // Defines how long to hold at max duty before ramping down.

static constexpr uint8_t kPwmStepUpPercent = 5U;                   // PWM ramp-up step size, per task spec (5% steps).
static constexpr uint8_t kPwmStepDownPercent = 10U;                // PWM ramp-down step size, per task spec (10% steps).
static constexpr uint8_t kPwmMaxPercent = 80U;                     // Maximum PWM duty percentage.
static constexpr uint8_t kPwmMinPercent = 0U;                      // Minimum PWM duty percentage.


int ApplyDuty(const device* pwm_dev, uint8_t duty_percent) {          // Applies a given duty percent to the PWM channel.

    const uint32_t pulse_ns = (kPwmPeriodNs * duty_percent) / 100U;      // Converts duty percent into a pulse width in nanoseconds.

    const int err =
        pwm_set(pwm_dev, kPwmChannel, kPwmPeriodNs, pulse_ns, PWM_POLARITY_NORMAL);
    // Applies 20 kHz period / requested duty cycle to TIM1_CH1 (PC0).

    if (err != 0) {                                                         // Checks whether the PWM call succeeded.

        LOG_ERR("pwm_set failed (err=%d) - halting", err);                     // Prints PWM configuration error.
    }

    return err;                                                             // Returns the pwm_set result to the caller.
}


}  // namespace                                                    // Closes private namespace.


int main() {                                                        // Main function starts the PWM ramp up/down task.

    LOG_INF("pwm ramp up/down @ 20kHz task booting");                // Prints startup message.

    const device* pwm_dev = DEVICE_DT_GET(DT_ALIAS(pwmtest));          // Gets the TIM1 PWM device from the board overlay.

    if (!device_is_ready(pwm_dev)) {                                    // Checks that the PWM device initialized correctly.

        LOG_ERR("TIM1 PWM device not ready - halting");                   // Prints PWM device error.

        return -1;                                                          // Stops program due to hardware failure.
    }

    while (true) {                                                            // Repeats the ramp up / hold / ramp down cycle forever.

        for (uint8_t duty_percent = kPwmMinPercent;                              // Ramps duty upward, 5% per step.
             duty_percent <= kPwmMaxPercent;
             duty_percent += kPwmStepUpPercent) {

            if (ApplyDuty(pwm_dev, duty_percent) != 0) {                            // Applies the current ramp-up step.

                return -1;                                                             // Stops program on PWM failure.
            }

            LOG_INF("pwm ramp up: %u%% duty @ 20kHz on TIM1_CH1 (PC0)", duty_percent);   // Confirms current ramp-up step is applied.

            k_msleep(kRampUpStepPeriodMs);                                          // Waits before applying the next ramp-up step.
        }

        LOG_INF("pwm holding: %u%% duty for %u ms", kPwmMaxPercent, kRampHoldPeriodMs);  // Confirms max duty is being held.

        k_msleep(kRampHoldPeriodMs);                                            // Holds at max duty for the configured stop period.

        for (int16_t duty_percent = kPwmMaxPercent;                             // Ramps duty downward, 10% per step.
             duty_percent >= kPwmMinPercent;
             duty_percent -= kPwmStepDownPercent) {

            if (ApplyDuty(pwm_dev, static_cast<uint8_t>(duty_percent)) != 0) {      // Applies the current ramp-down step.

                return -1;                                                             // Stops program on PWM failure.
            }

            LOG_INF("pwm ramp down: %d%% duty @ 20kHz on TIM1_CH1 (PC0)", duty_percent); // Confirms current ramp-down step is applied.

            k_msleep(kRampDownStepPeriodMs);                                        // Waits before applying the next ramp-down step.
        }
    }

    return 0;                                                                  // Unreachable, but ends main function cleanly.
}

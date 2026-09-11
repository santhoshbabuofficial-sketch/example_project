#include <zephyr/kernel.h>                                        // Includes Zephyr RTOS kernel functions like sleep.
#include <zephyr/device.h>                                        // Includes Zephyr device structure and APIs.
#include <zephyr/devicetree.h>                                    // Includes Zephyr device tree support.
#include <zephyr/drivers/pwm.h>                                   // Includes Zephyr PWM control functions.
#include <zephyr/logging/log.h>                                   // Includes Zephyr logging functions.


LOG_MODULE_REGISTER(pwm_fwd_rev_task, LOG_LEVEL_INF);             // Creates log module for this PWM task.


namespace {                                                        // Opens private namespace for local constants.


constexpr uint32_t kPwmPeriodNs = 50000U;                          // 20 kHz PWM period (1 / 20000 Hz = 50000 ns), shared by both channels.

constexpr uint32_t kForwardChannel = 1U;                          // TIM1_CH1 -> PC0.
constexpr uint32_t kReverseChannel = 2U;                          // TIM1_CH2 -> PC1.

constexpr uint32_t kForwardDutyPercent = 50U;                     // Forward channel duty cycle. Set the unused direction's duty to 0 so only one side ever drives.
constexpr uint32_t kReverseDutyPercent = 0U;                      // Reverse channel duty cycle - 0% while forward is active.


}  // namespace                                                    // Closes private namespace.


int main() {                                                        // Main function starts the PWM task.

    LOG_INF("pwm fwd/rev @ 20kHz task booting");                    // Prints startup message.

    const device* pwm_dev = DEVICE_DT_GET(DT_ALIAS(pwmtest));          // Gets the TIM1 PWM device from the board overlay.

    if (!device_is_ready(pwm_dev)) {                                    // Checks that the PWM device initialized correctly.

        LOG_ERR("TIM1 PWM device not ready - halting");                   // Prints PWM device error.

        return -1;                                                          // Stops program due to hardware failure.
    }

    const uint32_t forward_pulse_ns = (kPwmPeriodNs * kForwardDutyPercent) / 100U; // Converts forward duty into a pulse width in nanoseconds.
    const uint32_t reverse_pulse_ns = (kPwmPeriodNs * kReverseDutyPercent) / 100U; // Converts reverse duty into a pulse width in nanoseconds.

    const int fwd_err =
        pwm_set(pwm_dev, kForwardChannel, kPwmPeriodNs, forward_pulse_ns, PWM_POLARITY_NORMAL);
    // Applies 20 kHz period / forward duty cycle to TIM1_CH1 (PC0).

    if (fwd_err != 0) {                                                  // Checks whether the forward PWM call succeeded.

        LOG_ERR("pwm_set (forward) failed (err=%d) - halting", fwd_err);     // Prints forward PWM configuration error.

        return -1;                                                          // Stops program on failure.
    }

    const int rev_err =
        pwm_set(pwm_dev, kReverseChannel, kPwmPeriodNs, reverse_pulse_ns, PWM_POLARITY_NORMAL);
    // Applies 20 kHz period / reverse duty cycle to TIM1_CH2 (PC1).

    if (rev_err != 0) {                                                  // Checks whether the reverse PWM call succeeded.

        LOG_ERR("pwm_set (reverse) failed (err=%d) - halting", rev_err);     // Prints reverse PWM configuration error.

        return -1;                                                          // Stops program on failure.
    }

    LOG_INF("pwm running: fwd %u%% (PC0) / rev %u%% (PC1) @ 20kHz",
            kForwardDutyPercent, kReverseDutyPercent);                    // Confirms both channels are now running.

    while (true) {                                                        // Keeps the thread alive; PWM outputs are hardware-driven.

        k_msleep(1000);                                                    // Sleeps 1 second per idle loop iteration.
    }

    return 0;                                                              // Unreachable, but ends main function cleanly.
}

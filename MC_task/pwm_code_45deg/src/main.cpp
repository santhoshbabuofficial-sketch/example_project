#include <zephyr/kernel.h>                                        // Includes Zephyr RTOS kernel functions like sleep.
#include <zephyr/device.h>                                        // Includes Zephyr device structure and APIs.
#include <zephyr/devicetree.h>                                    // Includes Zephyr device tree support.
#include <zephyr/drivers/pwm.h>                                   // Includes Zephyr PWM control functions.
#include <zephyr/drivers/gpio.h>                                  // Includes Zephyr GPIO control functions - needed to drive PC2/PC3 high.
#include <zephyr/logging/log.h>                                   // Includes Zephyr logging functions.


LOG_MODULE_REGISTER(pwm_rotation_task, LOG_LEVEL_INF);            // Creates log module for this PWM task.


namespace {                                                        // Opens private namespace for local constants.


constexpr uint32_t kPwmPeriodNs = 50000U;                          // 20 kHz PWM period (1 / 20000 Hz = 50000 ns).
constexpr uint32_t kPwmChannel = 1U;                               // TIM1_CH1 -> PC0 output channel.


constexpr gpio_dt_spec kGpioPin2 = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), pin2_gpios); // PC2, per overlay.
constexpr gpio_dt_spec kGpioPin3 = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), pin3_gpios); // PC3, per overlay.

// --- Motor characteristics -----------------------------------------------
constexpr uint32_t kMotorRatedRpm = 320U;                          // Motor shaft speed at 100% duty 


constexpr uint32_t kTargetDegrees = 75U;                          // Output-shaft angle to turn per step.
constexpr uint32_t kDrivePercent = 20U;                           // PWM duty (= assumed % of rated speed) used while turning.
constexpr uint32_t kRepeatCount = 1U;                             // Number of 45-degree steps to run.
constexpr uint32_t kPauseMs = 1000U;                              // Stop time between each step.

// --- Derived timing -------------------------------------------------------
// time_ms = (angle * 6,000,000) / (360 * motor_rated_rpm * duty_percent)
// Derivation: output_rpm = motor_rpm * (duty/100)             <- no gearbox term now
//             time_per_rev_ms = 60000 / output_rpm
//             time_for_angle_ms = time_per_rev_ms * (angle / 360)
// uint64_t kept for headroom, though these input sizes no longer risk overflowing uint32_t.
constexpr uint64_t kRotationTimeMs =
    (static_cast<uint64_t>(kTargetDegrees) * 6000000ULL) /
    (360ULL * kMotorRatedRpm * kDrivePercent);                    // Milliseconds to hold the drive on for one kTargetDegrees step, at kDrivePercent duty.


}  // namespace                                                    // Closes private namespace.


int main() {                                                       

    LOG_INF("rotation task booting: %u deg/step, %u%% duty, %u steps",
            kTargetDegrees, kDrivePercent, kRepeatCount);              // Prints startup message with the run parameters.

    const device* pwm_dev = DEVICE_DT_GET(DT_ALIAS(pwmtest));          // Gets the TIM1 PWM device from the board overlay.

    if (!device_is_ready(pwm_dev)) {                                    // Checks that the PWM device initialized correctly.

        LOG_ERR("TIM1 PWM device not ready - halting");                   // Prints PWM device error.

        return -1;                                                          // Stops program due to hardware failure.
    }

    if (!gpio_is_ready_dt(&kGpioPin2)) {                                   // Checks that PC2's GPIO port initialized correctly.

        LOG_ERR("PC2 GPIO port not ready - halting");                         // Prints GPIO device error.

        return -1;                                                            // Stops program due to hardware failure.
    }

    if (!gpio_is_ready_dt(&kGpioPin3)) {                                   // Checks that PC3's GPIO port initialized correctly.

        LOG_ERR("PC3 GPIO port not ready - halting");                         // Prints GPIO device error.

        return -1;                                                            // Stops program due to hardware failure.
    }

    const int pc2_cfg_err = gpio_pin_configure_dt(&kGpioPin2, GPIO_OUTPUT_ACTIVE); // Configures PC2 as an output and immediately drives it high (per overlay's active-high flag).

    if (pc2_cfg_err != 0) {                                                // Checks whether configuring PC2 succeeded.

        LOG_ERR("PC2 configure failed (err=%d) - halting", pc2_cfg_err);      // Prints PC2 configuration error.

        return -1;                                                            // Stops program on failure.
    }

    const int pc3_cfg_err = gpio_pin_configure_dt(&kGpioPin3, GPIO_OUTPUT_ACTIVE); // Configures PC3 as an output and immediately drives it high (per overlay's active-high flag).

    if (pc3_cfg_err != 0) {                                                // Checks whether configuring PC3 succeeded.

        LOG_ERR("PC3 configure failed (err=%d) - halting", pc3_cfg_err);      // Prints PC3 configuration error.

        return -1;                                                            // Stops program on failure.
    }

    LOG_INF("PC2 and PC3 set high");                                       // Confirms both GPIO pins are now driven high.

    const uint32_t run_pulse_ns = (kPwmPeriodNs * kDrivePercent) / 100U;    // Converts drive duty into a pulse width in nanoseconds.

    for (uint32_t step = 1U; step <= kRepeatCount; ++step) {               // Runs kRepeatCount rotate-then-pause cycles.

        const int start_err =
            pwm_set(pwm_dev, kPwmChannel, kPwmPeriodNs, run_pulse_ns, PWM_POLARITY_NORMAL);
        // Starts the motor turning at kDrivePercent duty on TIM1_CH1 (PC0).

        if (start_err != 0) {                                               // Checks whether the PWM call succeeded.

            LOG_ERR("pwm_set (start) failed (err=%d) - halting", start_err);    // Prints PWM configuration error.

            return -1;                                                          // Stops program on failure.
        }

        LOG_INF("step %u/%u: turning %u deg (%llu ms)",
                step, kRepeatCount, kTargetDegrees, kRotationTimeMs);        // Logs which step is running and its calculated duration.

        k_msleep(static_cast<int32_t>(kRotationTimeMs));                    // Holds the drive on for the calculated single-step duration.

        const int stop_err =
            pwm_set(pwm_dev, kPwmChannel, kPwmPeriodNs, 0U, PWM_POLARITY_NORMAL);
        // Drops the pulse width to 0 - stops the motor at the end of this step.

        if (stop_err != 0) {                                                // Checks whether the stop call succeeded.

            LOG_ERR("pwm_set (stop) failed (err=%d)", stop_err);               // Prints PWM configuration error.

            return -1;                                                          // Stops program on failure.
        }

        LOG_INF("step %u/%u complete - motor stopped, pausing %u ms",
                step, kRepeatCount, kPauseMs);                              // Confirms this step finished and a pause is starting.

        k_msleep(kPauseMs);                                                 // Pauses between steps (also runs after the final step).
    }

    LOG_INF("all %u steps complete", kRepeatCount);                        // Confirms the whole sequence has finished.

    while (true) {                                                          // Keeps the thread alive after the sequence.

        k_msleep(1000);                                                      // Sleeps 1 second per idle loop iteration.
    }

    return 0;                                                                
}

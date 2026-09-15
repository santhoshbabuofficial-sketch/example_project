#include <zephyr/kernel.h>                                       // Includes Zephyr kernel thread and sleep functions.
#include <zephyr/logging/log.h>                                  // Includes Zephyr logging functions.

#include "bldc_driver.hpp"                                       // Includes inverter driver definition.
#include "commutation_table.hpp"                                 // Includes six-step commutation table.
#include "gpio_overlay.hpp"                                      // Includes hardware access object.
#include "speed_ramp.hpp"                                        // Includes open-loop speed profile.


LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);                        // Creates log module for the application entry point.


namespace {                                                       // Opens anonymous namespace for file-local objects.

constexpr uint32_t kStatusIntervalMs = 500U;
// How often the running speed is printed. Deliberately slow: logging from
// inside the commutation loop at 1.5 ms per step would flood the UART and
// distort the very timing being measured.

// Statically allocated - no heap use anywhere in this application.
motor_control::GpioOverlay g_overlay{};                           // Creates the central hardware access object.

motor_control::BldcDriver g_driver{g_overlay};                     // Creates the three-phase inverter driver.

motor_control::SpeedRamp g_ramp{};                                 // Creates the open-loop speed profile generator.

}  // namespace


int main() {                                                       // Application entry point.

    LOG_INF("Open-loop BLDC commutation starting");                // Prints application startup message.

    if (!g_overlay.Init()) {                                        // Initializes PWM device, enable pins, sync pins.

        LOG_ERR("Hardware init failed - halting");                  // Prints initialization failure.

        return -1;                                                   // Stops the application with the motor off.
    }

    if (!g_driver.Init()) {                                          // Initializes the inverter into all-off state.

        LOG_ERR("Inverter init failed - halting");                   // Prints initialization failure.

        return -1;                                                   // Stops the application with the motor off.
    }

    g_ramp.Start();                                                  // Begins the align -> accelerate -> run profile.

    uint8_t step_index = 0U;                                         // Tracks which of the six steps is active.

    int64_t last_status_ms = k_uptime_get();                         // Tracks when status was last printed.

    while (true) {                                                   // Runs the commutation loop forever.

        g_ramp.Update();                                             // Recomputes step period and duty for this moment.

        if (!g_driver.ApplyStep(step_index, g_ramp.duty_percent())) { // Applies the current commutation step.

            LOG_ERR("Commutation failed - cutting output");           // Prints the commutation failure.

            g_driver.AllOff();                                        // Tri-states every phase on any driver error.

            break;                                                    // Leaves the commutation loop.
        }

        // During align the rotor is being parked, so the step index is
        // deliberately NOT advanced - the same step is held until the align
        // window expires, which is what pulls the rotor to a known position.
        if (g_ramp.phase() != motor_control::RampPhase::kAlign) {

            step_index = static_cast<uint8_t>(
                (step_index + 1U) % motor_control::kStepsPerElectricalRev);
            // Advances to the next step, wrapping after six.
        }

        const int64_t now_ms = k_uptime_get();                        // Reads the current uptime.

        if ((now_ms - last_status_ms) >= static_cast<int64_t>(kStatusIntervalMs)) {

            LOG_INF("step=%u period=%u us duty=%u%% elec_rpm=%u",
                    step_index, g_ramp.step_period_us(),
                    g_ramp.duty_percent(), g_ramp.ElectricalRpm());
            // Prints the present operating point for monitoring.

            last_status_ms = now_ms;                                   // Records when status was printed.
        }

        k_usleep(static_cast<int32_t>(g_ramp.step_period_us()));       // Holds this step for its computed duration.
    }

    // Only reached on a driver failure. The motor is already coasting.
    while (true) {                                                     // Parks the application safely.

        k_msleep(1000);                                                // Sleeps indefinitely with the motor off.
    }

    return 0;                                                          // Never reached.
}

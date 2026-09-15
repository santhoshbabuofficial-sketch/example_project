#include <zephyr/kernel.h>                                       // Includes Zephyr kernel sleep and thread functions.
#include <zephyr/logging/log.h>                                  // Includes Zephyr logging functions.

#include "voltage_sensor.hpp"                                    // Includes VoltageSensor / VoltageMonitor definitions.


LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);                        // Creates log module for the application entry point.


namespace {                                                       // Opens anonymous namespace for file-local objects.

constexpr uint32_t kSupervisionIntervalMs = 100U;
// Voltage is re-evaluated 10x per second. Fast enough to react to a
// supply going out of range, slow enough that the averaged ADC read
// costs nothing meaningful.

// Statically allocated - no heap use anywhere in this application.
motor_control::VoltageSensor g_voltage_sensor{};                  // Creates the 0-25V module voltage sensor object.

motor_control::VoltageMonitor g_voltage_monitor{g_voltage_sensor}; // Creates the supervisor driving PC0 / PC1.

}  // namespace


int main() {                                                       // Application entry point.

    LOG_INF("Voltage supervision task starting (nominal 24 V)");   // Prints application startup message.

    if (!g_voltage_monitor.Init()) {                                // Initializes ADC path and both trigger pins.

        LOG_ERR("Voltage monitor init failed - halting");           // Prints initialization failure.

        // Init() leaves both trigger pins LOW on any failure path, so
        // halting here asserts neither "normal" nor "fault".
        return -1;                                                   // Stops the application.
    }

    while (true) {                                                   // Runs the supervision loop forever.

        static_cast<void>(g_voltage_monitor.Update());               // Reads voltage and updates the trigger pins.

        k_msleep(kSupervisionIntervalMs);                            // Waits before the next supervision cycle.
    }

    return 0;                                                        // Never reached.
}

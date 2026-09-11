#include <new>                                                   // Required for placement new (no-heap static-storage construction below).

#include <zephyr/kernel.h>                                      // Includes Zephyr RTOS kernel functions like threads and sleep.
#include <zephyr/logging/log.h>                                  // Includes Zephyr logging functions.

#include "current_control.hpp"                                   // Includes the closed-loop current controller.
#include "current_sensor.hpp"                                    // Includes current sensor class.
#include "fault_detection.hpp"                                   // Includes over-current fault detection class.
#include "gpio_overlay.hpp"                                      // Includes GPIO and hardware device control.
#include "pi_controller.hpp"                                     // Includes generic PI controller.


LOG_MODULE_REGISTER(current_control_task_main, LOG_LEVEL_INF);   // Creates log module for the current-control task main.


namespace {                                                       // Opens private namespace for local variables and functions.


using motor_control::CurrentLoopController;                  // Creates short name for the current loop controller class.
using motor_control::CurrentSensor;                           // Creates short name for current sensor class.
using motor_control::FaultDetector;                            // Creates short name for fault detector class.
using motor_control::GpioOverlay;                               // Creates short name for GPIO overlay class.
using motor_control::PiController;                               // Creates short name for PI controller class.


// ---- PI tuning (starting point - re-tune for your actual coil/driver) -----
// Kp: how hard duty reacts to the instantaneous error. Too high -> ringing
// or oscillation; too low -> sluggish response as current rises.
// Ki: how fast the integral term removes steady-state error. Too high ->
// overshoot and slow-decaying oscillation; too low -> current settles
// slightly off the limit and takes a long time to creep to it.
// Start with Ki = 0, raise Kp until the loop pulls duty down fast enough
// when current rises toward the limit, then raise Ki just enough to hold
// current steady at the limit without re-introducing oscillation.
constexpr float kPiKp = 15.0F;                                     // Proportional gain (duty% per amp of error).
constexpr float kPiKi = 40.0F;                                     // Integral gain (duty%/s per amp of error).

// This is the current limit: the loop starts open-loop at 50% duty, then
// closes around this target - if measured current rises above it, the PI
// term goes negative and pulls duty down until current settles back at
// (or below) this value.
constexpr float kCurrentLimitA = 0.2F;                              // Current limit / regulation target: 200 mA.

constexpr uint16_t kControlLoopPeriodMs = 2U;                       // Current-loop update period (500 Hz).
constexpr uint16_t kFaultCheckPeriodMs = 20U;                        // Fault-check period (separate from control-loop rate).

constexpr size_t kControlThreadStackSize = 2048U;                     // Memory size for the control-loop thread stack.
constexpr int kControlThreadPriority = 5;                               // Control-loop thread priority.


GpioOverlay g_overlay;                                                    // Creates GPIO/PWM/ADC hardware control object.

CurrentSensor* g_current_sensor = nullptr;                                 // Creates pointer for current sensor object.
PiController* g_pi = nullptr;                                               // Creates pointer for PI controller object.
CurrentLoopController* g_current_loop = nullptr;                             // Creates pointer for current loop controller object.
FaultDetector g_fault_detector;                                               // Creates fault detector object (no dynamic hardware needed).


alignas(CurrentSensor) uint8_t g_current_sensor_storage[sizeof(CurrentSensor)];
// Creates memory storage for current sensor object.

alignas(PiController) uint8_t g_pi_storage[sizeof(PiController)];
// Creates memory storage for PI controller object.

alignas(CurrentLoopController) uint8_t g_current_loop_storage[sizeof(CurrentLoopController)];
// Creates memory storage for current loop controller object.


void ControlThreadEntry(void*, void*, void*) {
// Zephyr thread function running the periodic PI current-control loop and
// the over-current fault check. No button, no manual trigger - this starts
// regulating the instant the thread runs.

    const float dt_s = static_cast<float>(kControlLoopPeriodMs) / 1000.0F;     // Precomputes the fixed control-loop sample time in seconds.

    uint16_t ms_since_fault_check = 0U;                                          // Tracks elapsed time toward the next fault check.

    while (true) {                                                               // Creates infinite RTOS task loop.

        g_current_loop->Tick(dt_s);                                               // Runs one PI current-loop step (measure -> PI -> apply duty).

        ms_since_fault_check =
            static_cast<uint16_t>(ms_since_fault_check + kControlLoopPeriodMs);
        // Advances the fault-check timer by one control-loop period.

        if (ms_since_fault_check >= kFaultCheckPeriodMs) {                          // Checks whether it is time for a fault check.

            ms_since_fault_check = 0U;                                               // Resets the fault-check timer.

            if (!g_fault_detector.IsFaulted() &&
                g_fault_detector.CheckCurrent(g_current_loop->measured_current())) {
                // Feeds the latest measured current into the fault detector.

                g_current_loop->EmergencyStop();                                        // Immediately disables the coil driver on over-current.

                LOG_ERR("Current loop stopped due to over-current fault");               // Logs the stop event.
            }
        }

        k_msleep(kControlLoopPeriodMs);                                              // Delays thread for the control-loop sample period.
    }
}


K_THREAD_STACK_DEFINE(g_control_thread_stack, kControlThreadStackSize);
// Creates stack memory for the control-loop RTOS thread.

k_thread g_control_thread{};                                                 // Creates control-loop thread control object.


}  // namespace                                                              // Closes private namespace.


int main() {                                                                 // Main function starts the current-control task.

    LOG_INF("current control task booting");                                    // Prints startup message.

    if (!g_overlay.Init()) {                                                   // Initializes GPIO/PWM/ADC hardware.

        LOG_ERR("gpio_overlay init failed - halting");                          // Prints initialization error.

        return -1;                                                               // Stops program due to hardware failure.
    }

    g_current_sensor = new (g_current_sensor_storage) CurrentSensor(g_overlay);
    // Creates current sensor object in preallocated memory.

    g_pi = new (g_pi_storage) PiController(kPiKp, kPiKi, 0.0F, 100.0F);
    // Creates the PI controller object in preallocated memory (output clamped 0-100%,
    // further clamped to 0-90% inside CurrentLoopController).

    g_current_loop =
        new (g_current_loop_storage) CurrentLoopController(g_overlay, *g_current_sensor, *g_pi);
    // Creates the closed-loop current controller object in preallocated memory.

    if (!g_current_sensor->Init() || !g_current_loop->Init()) {
    // Initializes current sensor and current-loop hardware.

        LOG_ERR("Peripheral init failed - halting");                             // Prints peripheral initialization error.

        return -1;                                                                 // Stops program on failure.
    }

    g_current_loop->SetReferenceCurrent(kCurrentLimitA);                            // Loads the fixed 200 mA current limit as the regulation target.

    g_current_loop->Enable();                                                        // Starts at 50% duty and immediately begins closing the loop -
                                                                                       // no button needed, this runs from boot.

    k_thread_create(&g_control_thread, g_control_thread_stack, kControlThreadStackSize,
                     ControlThreadEntry, nullptr, nullptr, nullptr, kControlThreadPriority, 0,
                     K_NO_WAIT);
    // Creates and starts the PI current-control RTOS thread.

    k_thread_name_set(&g_control_thread, "current_loop");                          // Names the control thread.

    LOG_INF("current control task running - limiting coil current to 200 mA");       // Prints running message.

    return 0;                                                                          // Ends main function successfully.
}

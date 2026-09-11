#include <new>                                                   // Required for placement new (no-heap static-storage construction below).

#include <zephyr/kernel.h>                                      // Includes Zephyr RTOS kernel functions like threads and sleep.
#include <zephyr/logging/log.h>                                  // Includes Zephyr logging functions.
#include <zephyr/drivers/gpio.h>                                 // Includes Zephyr GPIO control/interrupt functions.

#include "current_sensor.hpp"                                    // Includes current sensor class.
#include "fault_detection.hpp"                                   // Includes fault detection class.
#include "gpio_overlay.hpp"                                      // Includes GPIO and hardware device control.
#include "lcd_display.hpp"                                       // Includes LCD display control class.
#include "motor_control.hpp"                                     // Includes motor control state machine.
#include "sensor_manager.hpp"                                    // Includes sensor management class.
#include "voltage_sensor.hpp"                                    // Includes voltage sensor class.


LOG_MODULE_REGISTER(motor_control_task_main, LOG_LEVEL_INF);     // Creates log module for motor control task main.


namespace {                                                       // Opens private namespace for local variables and functions.


using motor_control::CurrentSensor;                          // Creates short name for current sensor class.
using motor_control::FaultDetector;                          // Creates short name for fault detector class.
using motor_control::FaultReason;                            // Creates short name for fault reason enum.
using motor_control::GpioOverlay;                             // Creates short name for GPIO overlay class.
using motor_control::LcdDisplay;                              // Creates short name for LCD display class.
using motor_control::LcdSensorObserver;                       // Creates short name for LCD observer class.
using motor_control::MotorControl;                            // Creates short name for motor control class.
using motor_control::SensorManager;                           // Creates short name for sensor manager class.
using motor_control::VoltageSensor;                           // Creates short name for voltage sensor class.


constexpr uint16_t kSensorSamplePeriodMs = 1000U;                 // Defines sensor reading period (also fault-check period).
constexpr uint16_t kRampStepPeriodMs = 400U;                      // Defines how often the PWM ramp advances by one 5% step.
constexpr uint16_t kButtonDebounceMs = 50U;                       // Defines minimum time between accepted button presses.

constexpr size_t kSensorThreadStackSize = 2048U;                  // Defines memory size for sensor thread stack.
constexpr size_t kRampThreadStackSize = 1024U;                    // Defines memory size for ramp thread stack.

constexpr int kSensorThreadPriority = 5;                           // Defines sensor thread priority.
constexpr int kRampThreadPriority = 5;                              // Defines ramp thread priority.


GpioOverlay g_overlay;                                              // Creates GPIO hardware control object.

VoltageSensor* g_voltage_sensor = nullptr;                          // Creates pointer for voltage sensor object.
CurrentSensor* g_current_sensor = nullptr;                           // Creates pointer for current sensor object.
LcdDisplay* g_lcd = nullptr;                                          // Creates pointer for LCD object.
LcdSensorObserver* g_lcd_observer = nullptr;                          // Creates pointer for LCD observer object.
SensorManager* g_sensor_manager = nullptr;                             // Creates pointer for sensor manager object.
MotorControl* g_motor = nullptr;                                        // Creates pointer for motor control object.
FaultDetector g_fault_detector;                                          // Creates fault detector object (no dynamic hardware needed).


alignas(VoltageSensor) uint8_t g_voltage_storage[sizeof(VoltageSensor)];
// Creates memory storage for voltage sensor object.

alignas(CurrentSensor) uint8_t g_current_storage[sizeof(CurrentSensor)];
// Creates memory storage for current sensor object.

alignas(LcdDisplay) uint8_t g_lcd_storage[sizeof(LcdDisplay)];
// Creates memory storage for LCD object.

alignas(LcdSensorObserver) uint8_t g_lcd_observer_storage[sizeof(LcdSensorObserver)];
// Creates memory storage for LCD observer object.

alignas(SensorManager) uint8_t g_sensor_manager_storage[sizeof(SensorManager)];
// Creates memory storage for sensor manager object.

alignas(MotorControl) uint8_t g_motor_storage[sizeof(MotorControl)];
// Creates memory storage for motor control object.


k_work g_button_work{};                                              // Creates Zephyr work item for deferred button handling.
gpio_callback g_button_cb{};                                          // Creates Zephyr GPIO callback structure for the button.

int64_t g_last_button_press_time_ms = 0;                                    // Stores timestamp of the last accepted button press.


const char* FaultReasonText(FaultReason reason) {                       // Converts a fault reason into LCD-friendly text.

    switch (reason) {

        case FaultReason::kOverVoltage:  return "OVER-VOLTAGE";           // Over-voltage fault text.
        case FaultReason::kOverCurrent:  return "OVER-CURRENT";            // Over-current fault text.
        case FaultReason::kNone:
        default:                         return "UNKNOWN";                  // Fallback text (should not normally occur).
    }
}


void ButtonWorkHandler(k_work*) {                              // Runs in thread context when a button press is processed.

    if (g_motor != nullptr) {                                             // Checks if motor control object exists.

        g_motor->OnButtonPress();                                          // Forwards the press to the ramp state machine.
    }
}


void ButtonIsr(const device* , gpio_callback* , uint32_t ) {

    const int64_t current_button_press_time_ms = k_uptime_get();                                 // Reads current uptime in milliseconds.

    if ((current_button_press_time_ms - g_last_button_press_time_ms) < kButtonDebounceMs) {            // Checks debounce window.

        return;                                                              // Ignores presses that are too close together.
    }

    g_last_button_press_time_ms = current_button_press_time_ms;                                         // Updates last accepted press timestamp.

    k_work_submit(&g_button_work);                                           // Defers actual handling to thread context.
}


void SensorThreadEntry(void* , void* , void* ) {
// Zephyr thread function for periodic sensor reading and fault checking.

    while (true) {                                                           // Creates infinite RTOS task loop.

        g_sensor_manager->Tick();                                             // Reads sensors and updates observers.

        if (!g_fault_detector.IsFaulted()) {                                   // Skips re-checking once a fault is latched.

            const bool over_voltage =
                g_fault_detector.CheckVoltage(g_sensor_manager->last_voltage_v());
            // Checks the latest voltage sample against the configured limit.

            const bool over_current =
                !over_voltage &&
                g_fault_detector.CheckCurrent(g_sensor_manager->last_current_a());
            // Checks the latest current sample (only if voltage did not already fault).

            if (over_voltage || over_current) {                                 // Checks if this sample newly triggered a fault.

                g_motor->EmergencyStop();                                         // Immediately stops the motor.

                g_lcd_observer->SetFaultActive(true);                              // Suppresses routine LCD row-0 updates.

                g_lcd->ShowFault(FaultReasonText(g_fault_detector.Reason()));       // Shows the fault on the LCD.

                LOG_ERR("Motor stopped due to fault");                              // Logs the stop event.
            }
        }

        k_msleep(kSensorSamplePeriodMs);                                       // Delays thread for sensor sample period.
    }
}


void RampThreadEntry(void* , void* , void*) {
// Zephyr thread function for advancing the PWM ramp and updating LCD row 1.

    while (true) {                                                             // Creates infinite RTOS task loop.

        g_motor->Tick();                                                        // Advances the ramp state machine by one step.

        if (!g_motor->is_faulted()) {                                            // Skips LCD update while a fault message is shown.

            g_lcd->ShowPwmStatus(g_motor->pwm_percent(),
                                  g_motor->direction() == motor_control::Direction::kForward);
            // Updates LCD row 1 with the current PWM duty and direction.
        }

        k_msleep(kRampStepPeriodMs);                                             // Delays thread for the ramp step period.
    }
}


K_THREAD_STACK_DEFINE(g_sensor_thread_stack, kSensorThreadStackSize);
// Creates stack memory for sensor RTOS thread.

K_THREAD_STACK_DEFINE(g_ramp_thread_stack, kRampThreadStackSize);
// Creates stack memory for ramp RTOS thread.

k_thread g_sensor_thread{};                                                 // Creates sensor thread control object.

k_thread g_ramp_thread{};                                                    // Creates ramp thread control object.


}  // namespace                                                              // Closes private namespace.


int main() {                                                                 // Main function starts the motor control task.

    LOG_INF("motor control task booting");                                    // Prints startup message.

    if (!g_overlay.Init()) {                                                   // Initializes GPIO/PWM/ADC/I2C hardware.

        LOG_ERR("gpio_overlay init failed - halting");                          // Prints initialization error.

        return -1;                                                               // Stops program due to hardware failure.
    }

    g_voltage_sensor = new (g_voltage_storage) VoltageSensor(g_overlay);
    // Creates voltage sensor object in preallocated memory.

    g_current_sensor = new (g_current_storage) CurrentSensor(g_overlay);
    // Creates current sensor object in preallocated memory.

    g_lcd = new (g_lcd_storage) LcdDisplay(g_overlay);
    // Creates LCD object in preallocated memory.

    g_lcd_observer = new (g_lcd_observer_storage) LcdSensorObserver(*g_lcd);
    // Creates LCD observer object in preallocated memory.

    g_sensor_manager =
        new (g_sensor_manager_storage) SensorManager(*g_voltage_sensor, *g_current_sensor);
    // Creates sensor manager object in preallocated memory.

    g_motor = new (g_motor_storage) MotorControl(g_overlay);
    // Creates motor control object in preallocated memory.

    if (!g_voltage_sensor->Init() || !g_current_sensor->Init() || !g_lcd->Init() ||
        !g_motor->Init()) {
    // Initializes voltage sensor, current sensor, LCD, and motor control hardware.

        LOG_ERR("Peripheral init failed - halting");                             // Prints peripheral initialization error.

        return -1;                                                                 // Stops program on failure.
    }

    g_sensor_manager->AddObserver(*g_lcd_observer);                                // Adds LCD observer for sensor updates.

    k_work_init(&g_button_work, ButtonWorkHandler);                                // Initializes deferred button work item.

    gpio_pin_interrupt_configure_dt(&g_overlay.user_button(), GPIO_INT_EDGE_TO_ACTIVE);
    // Configures the user button to interrupt on its active edge.

    gpio_init_callback(&g_button_cb, ButtonIsr, BIT(g_overlay.user_button().pin));
    // Sets up the GPIO callback structure for the user button pin.

    gpio_add_callback(g_overlay.user_button().port, &g_button_cb);
    // Registers the button callback with the GPIO driver.

    k_thread_create(&g_sensor_thread, g_sensor_thread_stack, kSensorThreadStackSize,
                     SensorThreadEntry, nullptr, nullptr, nullptr, kSensorThreadPriority, 0,
                     K_NO_WAIT);
    // Creates and starts sensor/fault reading RTOS thread.

    k_thread_name_set(&g_sensor_thread, "sensor_sample");                          // Names the sensor thread.

    k_thread_create(&g_ramp_thread, g_ramp_thread_stack, kRampThreadStackSize,
                     RampThreadEntry, nullptr, nullptr, nullptr, kRampThreadPriority, 0,
                     K_NO_WAIT);
    // Creates and starts the PWM ramp RTOS thread.

    k_thread_name_set(&g_ramp_thread, "motor_ramp");                                 // Names the ramp thread.

    LOG_INF("motor control task running");                                            // Prints running message.

    return 0;                                                                          // Ends main function successfully.
}
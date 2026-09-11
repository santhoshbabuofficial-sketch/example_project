#include "sensor_manager.hpp"                                // Includes SensorManager class definition.

#include <zephyr/logging/log.h>                             // Includes Zephyr logging functions.


LOG_MODULE_REGISTER(sensor_manager, LOG_LEVEL_INF);          // Creates log module for sensor manager.


namespace motor_control {                               // Opens motor control namespace.


SensorManager::SensorManager(IVoltageSensor& voltage_sensor,
                              ICurrentSensor& current_sensor)
    : voltage_sensor_(voltage_sensor),
      current_sensor_(current_sensor) {}
// Constructor connects sensor manager with voltage and current sensor drivers.


bool SensorManager::AddObserver(ISensorDataObserver& observer) {
// Adds a new observer to receive sensor data updates.

    if (observer_count_ >= kMaxObservers) {                    // Checks if observer list is full.

        return false;                                           // Returns failure because no space exists.
    }

    observers_[observer_count_] = &observer;                    // Stores observer address in array.

    ++observer_count_;                                           // Increases observer count.

    return true;                                                  // Returns successful observer addition.
}


void SensorManager::Tick() {
// Periodic function that reads sensors and updates observers.

    float voltage_v = 0.0F;                                        // Creates variable to store voltage value.

    if (voltage_sensor_.Read(voltage_v)) {                          // Reads voltage sensor value.

        last_voltage_v_ = voltage_v;                                  // Stores the latest raw voltage reading.

        for (std::size_t i = 0U; i < observer_count_; ++i) {           // Loops through all registered observers.

            observers_[i]->OnVoltageSample(voltage_v);                   // Sends voltage value to each observer.
        }

        LOG_INF("Voltage = %d.%02u V", static_cast<int>(voltage_v),
                static_cast<unsigned>((voltage_v - static_cast<int>(voltage_v)) * 100));
        // Prints voltage value.
    }


    float current_a = 0.0F;                                          // Creates variable to store current value.

    if (current_sensor_.Read(current_a)) {                            // Reads current sensor value.

        last_current_a_ = current_a;                                    // Stores the latest raw current reading.

        for (std::size_t i = 0U; i < observer_count_; ++i) {              // Loops through all registered observers.

            observers_[i]->OnCurrentSample(current_a);                      // Sends current value to each observer.
        }

        LOG_INF("Current = %d.%02u A", static_cast<int>(current_a),
                static_cast<unsigned>((current_a - static_cast<int>(current_a)) * 100));
        // Prints current value.
    }
}


}  // namespace mc::motor_control                                      

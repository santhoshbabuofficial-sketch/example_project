#pragma once                                              

#include <array>                                          // Includes fixed-size array container.
#include <cstddef>                                        // Includes size_t type.

#include "current_sensor.hpp"                             
#include "lcd_display.hpp"                           
#include "voltage_sensor.hpp"                              


namespace motor_control {                              // Opens motor control namespace.


class ISensorDataObserver {                                  // Creates interface for receiving sensor data.

public:

    virtual ~ISensorDataObserver() = default;                 // Creates virtual destructor.

    virtual void OnVoltageSample(float voltage_v) = 0;          // Receives new voltage sample.

    virtual void OnCurrentSample(float current_a) = 0;          // Receives new current sample.
};


// Forwards live voltage/current samples to LCD row 0. Suppressed while a
// fault is active so the fault message on the LCD is not overwritten by

class LcdSensorObserver final : public ISensorDataObserver {   // Creates LCD observer.

public:

    explicit LcdSensorObserver(ILcdDisplay& lcd) : lcd_(lcd) {}  // Constructor connects LCD object.

    void OnVoltageSample(float voltage_v) override {              // Updates cached voltage and refreshes LCD row 0.

        last_voltage_v_ = voltage_v;                                // Caches the latest voltage sample.

        if (!fault_active_) {                                        // Checks emergency stop / fault status.

            lcd_.ShowVoltageCurrent(last_voltage_v_, last_current_a_);  // Shows combined voltage/current line.
        }
    }

    void OnCurrentSample(float current_a) override {               // Updates cached current and refreshes LCD row 0.

        last_current_a_ = current_a;                                  // Caches the latest current sample.

        if (!fault_active_) {                                          // Checks emergency stop / fault status.

            lcd_.ShowVoltageCurrent(last_voltage_v_, last_current_a_);    // Shows combined voltage/current line.
        }
    }

    void SetFaultActive(bool active) { fault_active_ = active; }        // Updates fault-suppression state.


private:

    ILcdDisplay& lcd_;                                                    // Stores LCD reference.

    float last_voltage_v_ = 0.0F;                                          // Stores last cached voltage sample.

    float last_current_a_ = 0.0F;                                          // Stores last cached current sample.

    bool fault_active_ = false;                                             // Stores whether row 0 updates are suppressed.
};


// Reads both sensors once per Tick() and fans the raw values out to every
// registered observer (Observer pattern) - no heap allocation, fixed-size

class SensorManager {                                         // Manages sensors and observers.

public:

    static constexpr std::size_t kMaxObservers = 4U;            // Maximum observer count.

    SensorManager(IVoltageSensor& voltage_sensor,
                  ICurrentSensor& current_sensor);               // Connects voltage and current sensors.

    bool AddObserver(ISensorDataObserver& observer);             // Registers a new observer.

    void Tick();                                                  // Reads sensors and updates observers.

    float last_voltage_v() const { return last_voltage_v_; }      // Returns the most recent voltage reading.

    float last_current_a() const { return last_current_a_; }       // Returns the most recent current reading.


private:

    IVoltageSensor& voltage_sensor_;                                 // Stores voltage sensor reference.

    ICurrentSensor& current_sensor_;                                  // Stores current sensor reference.

    std::array<ISensorDataObserver*, kMaxObservers> observers_{};      // Stores registered observer pointers.

    std::size_t observer_count_ = 0U;                                  // Stores current observer count.

    float last_voltage_v_ = 0.0F;                                       // Stores last successfully read voltage.

    float last_current_a_ = 0.0F;                                        // Stores last successfully read current.
};


}  // namespace mc::motor_control                                     
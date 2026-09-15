#pragma once

#include <cstdint>

#include "hall_sensor.hpp"
#include "phase_driver.hpp"

namespace bldc {

// Sensored six-step trapezoidal BLDC controller. Unlike the sensorless
// version, hall sensors give absolute rotor position immediately, so
// there's no align/ramp: the first hall code read is commutated directly,
// and every subsequent hall transition commutates again.
class MotorController {
public:
    MotorController(PhaseDriver& driver, HallSensor& hall);

    [[nodiscard]] bool Init();

    // Reads the current hall code and applies the matching commutation
    // step. Call once at startup, and again from OnHallChange() on every
    // hall transition.
    void Commutate();

    // Call from a deferred (thread/workqueue) context whenever any hall
    // pin transitions - see main.cpp for why this isn't called directly
    // from the GPIO ISR.
    void OnHallChange();

    // Call periodically (e.g. every 100 ms from a k_timer). If the motor
    // is expected to be running but no hall transition has been seen
    // within the stall timeout, cuts all phases - catches a stalled
    // rotor or a disconnected/powered-down hall sensor.
    void CheckStall();

private:
    void Fault(const char* reason);

    PhaseDriver& driver_;
    HallSensor& hall_;
    int64_t last_transition_uptime_ = 0;
    bool running_ = false;
};

}  // namespace bldc

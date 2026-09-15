#pragma once

#include <cstdint>

#include "bemf_sensor.hpp"
#include "commutation_table.hpp"
#include "phase_driver.hpp"

namespace bldc {

// Sensorless six-step trapezoidal BLDC controller:
//   1. Align  - force one commutation step for a fixed time to park the
//               rotor at a known electrical position.
//   2. Ramp   - force commutation at a decreasing, open-loop interval
//               until the motor is spinning fast enough for BEMF to be
//               readable.
//   3. Closed loop - commutate from ADC-sensed zero crossings, timed 30
//               electrical degrees after each detected crossing.
// This is a starting point, not a tuned controller: align/ramp timing,
// duty cycles, and the stall timeout are all placeholders in
// motor_controller.cpp and will need adjusting against your motor and a
// scope on the floating-phase / commutation signals.
class MotorController {
public:
    MotorController(PhaseDriver& driver, BemfSensor& sensor);

    [[nodiscard]] bool Init();

    // Blocking: runs Align -> OpenLoopRamp -> ClosedLoop. Only returns on
    // an unrecoverable fault (driver or ADC failure); all phases are off
    // by the time it returns.
    void Run();

private:
    void Align();
    void OpenLoopRamp();
    void ClosedLoop();
    void Fault(const char* reason);

    PhaseDriver& driver_;
    BemfSensor& sensor_;
    uint8_t step_index_ = 0U;
};

}  // namespace bldc

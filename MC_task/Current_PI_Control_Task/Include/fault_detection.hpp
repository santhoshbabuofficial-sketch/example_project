#pragma once

namespace motor_control {                              // Opens motor control namespace.


// Simple latching over-current guard. No voltage monitoring in this task -
// there is no voltage sensor here, only the current loop.
//
// This is a hard safety ceiling, separate from the PI loop's 200 mA
// regulation target: the PI loop is expected to keep current AT the
// reference by pulling duty down as current rises toward it. This detector
// only trips if the loop fails to do that fast enough (e.g. a stall or a
// sensor fault) and current keeps climbing past a safe margin above the
// reference. Adjust kMaxCurrentA to whatever margin your hardware needs.
class FaultDetector {                                        // Creates class to monitor the coil current limit.

public:

    static constexpr float kMaxCurrentA = 0.5F;                // Hard safety ceiling (2.5x the 200 mA regulation target).

    // Feeds one new current sample. Returns true if this sample newly
    // triggers a fault (was previously clear).
    bool CheckCurrent(float current_a);

    bool IsFaulted() const { return active_; }                // Returns whether a fault is currently latched.

    void Reset();                                               // Clears the latched fault (call only after a safe restart).


private:

    bool active_ = false;                                       // Stores whether a fault is currently latched.
};


}  // namespace motor_control

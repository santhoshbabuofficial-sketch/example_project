#pragma once                                              // Prevents this header file from being included multiple times.


namespace mc::motor_control {                              // Opens motor control namespace.


enum class FaultReason {                                    // Enumerates possible fault causes.

    kNone,                                                   // No fault present.
    kOverVoltage,                                             // Bus voltage exceeded the configured limit.
    kOverCurrent,                                              // Motor current exceeded the configured limit.
};


// Latching threshold monitor: once tripped, stays tripped until Reset() is
// called explicitly (e.g. after the user acknowledges the fault). This
// avoids the motor silently re-enabling itself the instant a reading dips
// back under the limit for one sample.
class FaultDetector {                                        // Creates class to monitor voltage/current limits.

public:

    static constexpr float kMaxVoltageV = 14.0F;              // Maximum allowed bus voltage before fault.
    static constexpr float kMaxCurrentA = 3.0F;                // Maximum allowed motor current before fault.

    // Feeds one new voltage sample. Returns true if this sample newly
    // triggers a fault (was previously clear).
    bool CheckVoltage(float voltage_v);

    // Feeds one new current sample. Returns true if this sample newly
    // triggers a fault (was previously clear).
    bool CheckCurrent(float current_a);

    bool IsFaulted() const { return active_; }                // Returns whether a fault is currently latched.

    FaultReason Reason() const { return reason_; }             // Returns the reason for the latched fault.

    void Reset();                                               // Clears the latched fault (call only after a safe restart).


private:

    bool active_ = false;                                       // Stores whether a fault is currently latched.

    FaultReason reason_ = FaultReason::kNone;                    // Stores the reason for the latched fault.
};


}  // namespace mc::motor_control                              // Closes motor control namespace.

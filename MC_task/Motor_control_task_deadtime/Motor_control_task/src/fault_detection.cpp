#include "fault_detection.hpp"                                   

#include <zephyr/logging/log.h>                                   


LOG_MODULE_REGISTER(fault_detection, LOG_LEVEL_INF);              // Creates log module for fault detection.


namespace motor_control {                                    // Opens motor control namespace.


bool FaultDetector::CheckVoltage(float voltage_v) {               // Checks a new voltage sample against the limit.

    if (active_) {                                                 // Checks whether a fault is already latched.

        return false;                                              // Does not re-trigger an already-latched fault.
    }

    if (voltage_v > kMaxVoltageV) {                                // Checks whether voltage exceeds the limit.

        active_ = true;                                            // Latches the fault.
        reason_ = FaultReason::kOverVoltage;                        // Records the fault reason.

        LOG_ERR("FAULT: over-voltage %d.%02u V (limit %d V)",
                static_cast<int>(voltage_v),
                static_cast<unsigned>((voltage_v - static_cast<int>(voltage_v)) * 100),
                static_cast<int>(kMaxVoltageV));
        // Prints over-voltage fault details.

        return true;                                                // Reports a newly-triggered fault.
    }

    return false;                                                  // No fault on this sample.
}


bool FaultDetector::CheckCurrent(float current_a) {                 // Checks a new current sample against the limit.

    if (active_) {                                                  // Checks whether a fault is already latched.

        return false;                                               // Does not re-trigger an already-latched fault.
    }

    if (current_a > kMaxCurrentA) {                                  // Checks whether current exceeds the limit.

        active_ = true;                                              // Latches the fault.
        reason_ = FaultReason::kOverCurrent;                          // Records the fault reason.

        LOG_ERR("FAULT: over-current %d.%02u A (limit %d A)",
                static_cast<int>(current_a),
                static_cast<unsigned>((current_a - static_cast<int>(current_a)) * 100),
                static_cast<int>(kMaxCurrentA));
        // Prints over-current fault details.

        return true;                                                  // Reports a newly-triggered fault.
    }

    return false;                                                    // No fault on this sample.
}


void FaultDetector::Reset() {                                        // Clears the latched fault.

    active_ = false;                                                   // Clears fault-active flag.
    reason_ = FaultReason::kNone;                                       // Clears fault reason.
}


}  // namespace mc::motor_control                                    

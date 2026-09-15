#include "motor_controller.hpp"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "hall_commutation_table.hpp"

LOG_MODULE_REGISTER(bldc_motor_controller, LOG_LEVEL_INF);

namespace bldc {
namespace {

constexpr uint32_t kDutyPercent = 35U;  // Placeholder - tune for your motor/load.
constexpr int64_t kStallTimeoutMs = 500;

}  // namespace

MotorController::MotorController(PhaseDriver& driver, HallSensor& hall)
    : driver_(driver), hall_(hall) {}

bool MotorController::Init() {
    if (!driver_.Init()) {
        return false;
    }
    driver_.AllOff();
    return true;
}

void MotorController::Commutate() {
    const uint8_t code = hall_.ReadCode();
    const auto& step = kHallCommutationTable[code];

    if (!step.valid) {
        Fault("invalid hall code (0 or 7) - check hall wiring/power");
        return;
    }

    if (!driver_.ApplyStep(step.high_phase, step.low_phase, step.floating_phase, kDutyPercent)) {
        Fault("phase driver failure");
        return;
    }

    running_ = true;
    last_transition_uptime_ = k_uptime_get();
}

void MotorController::OnHallChange() {
    Commutate();
}

void MotorController::CheckStall() {
    if (!running_) {
        return;
    }
    if ((k_uptime_get() - last_transition_uptime_) > kStallTimeoutMs) {
        Fault("stall detected: no hall transition within timeout");
    }
}

void MotorController::Fault(const char* reason) {
    LOG_ERR("Motor fault: %s - cutting all phases", reason);
    driver_.AllOff();
    running_ = false;
}

}  // namespace bldc

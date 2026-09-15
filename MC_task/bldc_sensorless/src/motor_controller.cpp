#include "motor_controller.hpp"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(bldc_motor_controller, LOG_LEVEL_INF);

namespace bldc {
namespace {

// --- Tunables - placeholders, adjust for your motor/driver -------------
constexpr uint32_t kAlignDutyPercent = 20U;
constexpr uint32_t kAlignDurationMs = 500U;

constexpr uint32_t kOpenLoopDutyPercent = 25U;
constexpr uint32_t kOpenLoopStartPeriodUs = 8000U;  // Slow first steps.
constexpr uint32_t kOpenLoopEndPeriodUs = 1500U;    // Speed to hand off at.
constexpr uint32_t kOpenLoopPeriodDecrementUs = 40U;

constexpr uint32_t kClosedLoopDutyPercent = 35U;
constexpr uint32_t kZeroCrossSampleIntervalUs = 50U;  // 20 kHz BEMF poll rate.
constexpr int64_t kStallTimeoutMs = 500;

}  // namespace

MotorController::MotorController(PhaseDriver& driver, BemfSensor& sensor)
    : driver_(driver), sensor_(sensor) {}

bool MotorController::Init() {
    if (!driver_.Init()) {
        return false;
    }
    if (!sensor_.Init()) {
        return false;
    }
    driver_.AllOff();
    return true;
}

void MotorController::Align() {
    LOG_INF("Aligning rotor");
    step_index_ = 0U;
    const auto& step = kCommutationTable[step_index_];
    static_cast<void>(
        driver_.ApplyStep(step.high_phase, step.low_phase, step.floating_phase, kAlignDutyPercent));
    k_msleep(kAlignDurationMs);
}

void MotorController::OpenLoopRamp() {
    LOG_INF("Open-loop ramp");
    uint32_t period_us = kOpenLoopStartPeriodUs;

    while (period_us > kOpenLoopEndPeriodUs) {
        step_index_ = static_cast<uint8_t>((step_index_ + 1U) % kCommutationTable.size());
        const auto& step = kCommutationTable[step_index_];
        if (!driver_.ApplyStep(step.high_phase, step.low_phase, step.floating_phase,
                                kOpenLoopDutyPercent)) {
            Fault("phase driver failure during ramp");
            return;
        }
        k_usleep(static_cast<int32_t>(period_us));
        period_us -= kOpenLoopPeriodDecrementUs;
    }

    LOG_INF("Ramp complete, handing off to closed-loop BEMF sensing");
}

void MotorController::ClosedLoop() {
    LOG_INF("Closed-loop BEMF commutation");
    sensor_.ResetEdgeTracking();

    int64_t last_zero_cross_uptime = k_uptime_get();
    int64_t last_step_uptime = last_zero_cross_uptime;

    while (true) {
        k_usleep(kZeroCrossSampleIntervalUs);

        if (!sensor_.SampleAll()) {
            Fault("BEMF ADC sample failure");
            return;
        }

        const auto& step = kCommutationTable[step_index_];
        const bool zero_cross = sensor_.ZeroCrossDetected(step.floating_phase, step.high_phase,
                                                            step.low_phase, step.expected_edge);
        const int64_t now = k_uptime_get();

        if (zero_cross) {
            const int64_t interval_ms = now - last_zero_cross_uptime;
            last_zero_cross_uptime = now;

            // Commutate 30 electrical degrees after the zero cross, i.e.
            // half of the last zero-cross-to-zero-cross interval.
            if (interval_ms > 0) {
                k_msleep(static_cast<int32_t>(interval_ms / 2));
            }

            step_index_ = static_cast<uint8_t>((step_index_ + 1U) % kCommutationTable.size());
            const auto& next_step = kCommutationTable[step_index_];
            if (!driver_.ApplyStep(next_step.high_phase, next_step.low_phase,
                                    next_step.floating_phase, kClosedLoopDutyPercent)) {
                Fault("phase driver failure in closed loop");
                return;
            }
            sensor_.ResetEdgeTracking();
            last_step_uptime = k_uptime_get();
        } else if ((now - last_step_uptime) > kStallTimeoutMs) {
            Fault("stall detected: no zero cross within timeout");
            return;
        }
    }
}

void MotorController::Fault(const char* reason) {
    LOG_ERR("Motor fault: %s - cutting all phases", reason);
    driver_.AllOff();
}

void MotorController::Run() {
    Align();
    OpenLoopRamp();
    ClosedLoop();
}

}  // namespace bldc

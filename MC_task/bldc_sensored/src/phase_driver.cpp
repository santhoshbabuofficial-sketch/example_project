#include "phase_driver.hpp"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(bldc_phase_driver, LOG_LEVEL_INF);

namespace bldc {

PhaseDriver::PhaseDriver(const std::array<PhaseHw, 3U>& phases) : phases_(phases) {}

bool PhaseDriver::Init() {
    for (const auto& hw : phases_) {
        if (!pwm_is_ready_dt(&hw.pwm)) {
            LOG_ERR("Phase PWM device not ready");
            return false;
        }
        if (!gpio_is_ready_dt(&hw.enable)) {
            LOG_ERR("Phase enable GPIO not ready");
            return false;
        }
        if (gpio_pin_configure_dt(&hw.enable, GPIO_OUTPUT_INACTIVE) != 0) {
            LOG_ERR("Failed to configure phase enable GPIO");
            return false;
        }
    }
    return true;
}

bool PhaseDriver::DriveHigh(Phase phase, uint32_t duty_percent) {
    const auto idx = static_cast<uint8_t>(phase);
    if (gpio_pin_set_dt(&phases_[idx].enable, 1) != 0) {
        return false;
    }
    const uint32_t pulse_ns = (phases_[idx].pwm.period * duty_percent) / 100U;
    return pwm_set_pulse_dt(&phases_[idx].pwm, pulse_ns) == 0;
}

bool PhaseDriver::DriveLow(Phase phase) {
    const auto idx = static_cast<uint8_t>(phase);
    if (gpio_pin_set_dt(&phases_[idx].enable, 1) != 0) {
        return false;
    }
    return pwm_set_pulse_dt(&phases_[idx].pwm, 0U) == 0;
}

bool PhaseDriver::Float(Phase phase) {
    const auto idx = static_cast<uint8_t>(phase);
    // Zero the duty first so nothing is driven the instant enable drops.
    static_cast<void>(pwm_set_pulse_dt(&phases_[idx].pwm, 0U));
    return gpio_pin_set_dt(&phases_[idx].enable, 0) == 0;
}

bool PhaseDriver::ApplyStep(Phase high_phase, Phase low_phase, Phase floating_phase,
                             uint32_t duty_percent) {
    // Float the outgoing phase first so it never overlaps a driven state.
    if (!Float(floating_phase)) {
        return false;
    }
    if (!DriveLow(low_phase)) {
        return false;
    }
    return DriveHigh(high_phase, duty_percent);
}

void PhaseDriver::AllOff() {
    for (uint8_t i = 0U; i < phases_.size(); ++i) {
        static_cast<void>(Float(static_cast<Phase>(i)));
    }
}

}  // namespace bldc

#include "bldc_driver.hpp"                                         // Class declaration.

#include <zephyr/devicetree.h>                                     // DT_ALIAS / DT_NODELABEL macros.
#include <zephyr/logging/log.h>                                    // Logging macros.

LOG_MODULE_REGISTER(bldc_driver, LOG_LEVEL_INF);                   // Log module for the inverter driver layer.

namespace bldc {

namespace {                                                        // Private linkage for the devicetree bindings.

// --- PWM inputs: TIM1_CH1/CH2/CH3 -> PC0 / PC1 / PC2 -----------------------
// All three channels share TIM1, so they share one period and stay phase
// coherent - exactly what a three-phase inverter wants.
const pwm_dt_spec kPhaseA = PWM_DT_SPEC_GET_BY_NAME(DT_PATH(bldc_motor), phase_a);
const pwm_dt_spec kPhaseB = PWM_DT_SPEC_GET_BY_NAME(DT_PATH(bldc_motor), phase_b);
const pwm_dt_spec kPhaseC = PWM_DT_SPEC_GET_BY_NAME(DT_PATH(bldc_motor), phase_c);

// --- Shutdown / enable pins: one per IR2104 --------------------------------
// The overlay declares these GPIO_ACTIVE_LOW because IR2104 SD is active-low
// in silicon. Zephyr's logical-level API means gpio_pin_set(..., 1) always
// reads as "bridge active" here, whatever the electrical polarity.
const gpio_dt_spec kEnableA = GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(bldc_motor), enable_gpios, 0);
const gpio_dt_spec kEnableB = GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(bldc_motor), enable_gpios, 1);
const gpio_dt_spec kEnableC = GPIO_DT_SPEC_GET_BY_IDX(DT_PATH(bldc_motor), enable_gpios, 2);

// --- Scope trigger ---------------------------------------------------------
const gpio_dt_spec kSyncPin = GPIO_DT_SPEC_GET(DT_PATH(bldc_motor), sync_gpios);

constexpr uint32_t kPwmPeriodNs = 50000U;                          // 20 kHz carrier - above audible, easy to filter on a scope.

}  // namespace

int BldcDriver::init() noexcept {

    const pwm_dt_spec* const pwm_channels[3] = {&kPhaseA, &kPhaseB, &kPhaseC};   // Iterate the three PWM channels.

    for (const pwm_dt_spec* const channel : pwm_channels) {                       // Validate each PWM binding.
        if (!pwm_is_ready_dt(channel)) {
            LOG_ERR("PWM channel not ready (dev=%s ch=%u)",
                    (channel->dev != nullptr) ? channel->dev->name : "<null>",
                    channel->channel);
            return -ENODEV;                                                       // Refuse to run with half a bridge.
        }
    }

    const gpio_dt_spec* const gpio_outputs[4] = {&kEnableA, &kEnableB, &kEnableC, &kSyncPin};  // All GPIO outputs.

    for (const gpio_dt_spec* const pin : gpio_outputs) {                          // Validate and configure each GPIO.
        if (!gpio_is_ready_dt(pin)) {
            LOG_ERR("GPIO not ready (port=%s pin=%u)",
                    (pin->port != nullptr) ? pin->port->name : "<null>", pin->pin);
            return -ENODEV;
        }

        const int err = gpio_pin_configure_dt(pin, GPIO_OUTPUT_INACTIVE);         // Start inactive: bridges disabled.
        if (err != 0) {
            LOG_ERR("gpio_pin_configure_dt failed (pin=%u err=%d)", pin->pin, err);
            return err;
        }
    }

    ready_ = true;                                                                // Hardware validated.

    coast();                                                                      // Guarantee a known safe state before returning.

    LOG_INF("inverter ready: 20 kHz carrier, 3x half-bridge, six-step table");

    return 0;
}

int BldcDriver::set_phase(const pwm_dt_spec& pwm_ch,
                          const gpio_dt_spec& enable_pin,
                          PhaseState state,
                          uint32_t duty_percent) noexcept {

    uint32_t pulse_ns = 0U;                                                       // Default: IN low.
    bool enable = false;                                                          // Default: bridge disabled.

    switch (state) {
        case PhaseState::High:                                                    // Sourcing phase: chop the high side.
            pulse_ns = duty_to_pulse_ns(kPwmPeriodNs, duty_percent);
            enable   = true;
            break;

        case PhaseState::Low:                                                     // Sinking phase: IN held low, low side fully on.
            pulse_ns = 0U;
            enable   = true;
            break;

        case PhaseState::Float:                                                   // Idle phase: bridge off, winding high-Z.
        default:
            pulse_ns = 0U;
            enable   = false;
            break;
    }

    const int pwm_err = pwm_set_dt(&pwm_ch, kPwmPeriodNs, pulse_ns);              // Program the timer channel.
    if (pwm_err != 0) {
        LOG_ERR("pwm_set_dt failed (ch=%u err=%d)", pwm_ch.channel, pwm_err);
        return pwm_err;
    }

    const int gpio_err = gpio_pin_set_dt(&enable_pin, enable ? 1 : 0);            // Then enable or disable the bridge.
    if (gpio_err != 0) {
        LOG_ERR("gpio_pin_set_dt failed (pin=%u err=%d)", enable_pin.pin, gpio_err);
        return gpio_err;
    }

    return 0;
}

int BldcDriver::apply_step(uint8_t step_index, uint32_t duty_percent) noexcept {

    if (!ready_) {                                                                // Never touch hardware before init().
        return -EPERM;
    }

    if (step_index >= kStepsPerElectricalRev) {                                   // Guard against a corrupt step counter.
        LOG_ERR("invalid step index %u", step_index);
        coast();
        return -EINVAL;
    }

    const CommutationStep& step = kCommutationTable[step_index];                  // Look up the phase pattern.

    // Phase 1: drop the enables of every bridge that must float in the new
    // step *before* reprogramming anything, so no winding is ever driven with
    // a stale pattern during the transition.
    if (step.phase_a == PhaseState::Float) { (void)gpio_pin_set_dt(&kEnableA, 0); }
    if (step.phase_b == PhaseState::Float) { (void)gpio_pin_set_dt(&kEnableB, 0); }
    if (step.phase_c == PhaseState::Float) { (void)gpio_pin_set_dt(&kEnableC, 0); }

    // Phase 2: program duty and enable for each phase in turn.
    int err = set_phase(kPhaseA, kEnableA, step.phase_a, duty_percent);
    if (err == 0) { err = set_phase(kPhaseB, kEnableB, step.phase_b, duty_percent); }
    if (err == 0) { err = set_phase(kPhaseC, kEnableC, step.phase_c, duty_percent); }

    if (err != 0) {                                                               // Any failure mid-step is unsafe.
        LOG_ERR("apply_step(%u) failed (err=%d) - coasting", step_index, err);
        coast();
    }

    return err;
}

void BldcDriver::coast() noexcept {

    (void)gpio_pin_set_dt(&kEnableA, 0);                                          // Disable bridge A.
    (void)gpio_pin_set_dt(&kEnableB, 0);                                          // Disable bridge B.
    (void)gpio_pin_set_dt(&kEnableC, 0);                                          // Disable bridge C.

    (void)pwm_set_dt(&kPhaseA, kPwmPeriodNs, 0U);                                 // Zero duty on A.
    (void)pwm_set_dt(&kPhaseB, kPwmPeriodNs, 0U);                                 // Zero duty on B.
    (void)pwm_set_dt(&kPhaseC, kPwmPeriodNs, 0U);                                 // Zero duty on C.
}

void BldcDriver::pulse_sync(bool level) noexcept {
    (void)gpio_pin_set_dt(&kSyncPin, level ? 1 : 0);                              // Scope trigger marker.
}

}  // namespace bldc

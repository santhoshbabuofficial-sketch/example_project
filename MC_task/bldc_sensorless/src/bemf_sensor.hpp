#pragma once

#include <array>
#include <cstdint>

#include <zephyr/drivers/adc.h>

#include "bldc_types.hpp"

namespace bldc {

// Samples all three phase voltages and detects the back-EMF zero cross on
// the currently floating phase, using a software "virtual neutral" formed
// from the average of the two actively-driven phases. This avoids needing
// a dedicated neutral-point resistor network, at the cost of some noise
// sensitivity right at commutation edges - tune kZeroCrossSampleIntervalUs
// in motor_controller.cpp and/or add hardware low-pass filtering on the
// divider network if zero crossings look noisy on a scope.
class BemfSensor {
public:
    explicit BemfSensor(const std::array<adc_dt_spec, 3U>& channels);

    [[nodiscard]] bool Init();

    // Samples all three phase-voltage ADC channels. Call once per control
    // loop tick, before ZeroCrossDetected().
    [[nodiscard]] bool SampleAll();

    // Must be called whenever the floating phase changes (i.e. right after
    // a commutation), so the previous sample isn't compared against a
    // different phase's voltage.
    void ResetEdgeTracking();

    // Returns true the first tick the floating phase's voltage crosses the
    // virtual neutral in the expected direction.
    [[nodiscard]] bool ZeroCrossDetected(Phase floating_phase, Phase high_phase,
                                          Phase low_phase, ZeroCrossEdge expected_edge);

private:
    std::array<adc_dt_spec, 3U> channels_;
    std::array<int16_t, 3U> raw_samples_{};
    int32_t previous_relative_ = 0;
    bool have_previous_sample_ = false;
};

}  // namespace bldc

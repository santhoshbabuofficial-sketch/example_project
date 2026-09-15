#include "bemf_sensor.hpp"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(bldc_bemf_sensor, LOG_LEVEL_INF);

namespace bldc {

BemfSensor::BemfSensor(const std::array<adc_dt_spec, 3U>& channels) : channels_(channels) {}

bool BemfSensor::Init() {
    for (const auto& ch : channels_) {
        if (!adc_is_ready_dt(&ch)) {
            LOG_ERR("BEMF ADC channel not ready");
            return false;
        }
        if (adc_channel_setup_dt(&ch) != 0) {
            LOG_ERR("BEMF ADC channel setup failed");
            return false;
        }
    }
    return true;
}

bool BemfSensor::SampleAll() {
    for (uint8_t i = 0U; i < channels_.size(); ++i) {
        adc_sequence seq{};
        seq.buffer = &raw_samples_[i];
        seq.buffer_size = sizeof(raw_samples_[i]);

        if (adc_sequence_init_dt(&channels_[i], &seq) != 0) {
            return false;
        }
        if (adc_read_dt(&channels_[i], &seq) != 0) {
            return false;
        }
    }
    return true;
}

void BemfSensor::ResetEdgeTracking() {
    have_previous_sample_ = false;
}

bool BemfSensor::ZeroCrossDetected(Phase floating_phase, Phase high_phase, Phase low_phase,
                                    ZeroCrossEdge expected_edge) {
    const auto f_idx = static_cast<uint8_t>(floating_phase);
    const auto h_idx = static_cast<uint8_t>(high_phase);
    const auto l_idx = static_cast<uint8_t>(low_phase);

    const int32_t neutral =
        (static_cast<int32_t>(raw_samples_[h_idx]) + static_cast<int32_t>(raw_samples_[l_idx])) /
        2;
    const int32_t relative = static_cast<int32_t>(raw_samples_[f_idx]) - neutral;

    if (!have_previous_sample_) {
        previous_relative_ = relative;
        have_previous_sample_ = true;
        return false;
    }

    bool crossed = false;
    if (expected_edge == ZeroCrossEdge::kRising) {
        crossed = (previous_relative_ < 0) && (relative >= 0);
    } else {
        crossed = (previous_relative_ > 0) && (relative <= 0);
    }

    previous_relative_ = relative;
    return crossed;
}

}  // namespace bldc

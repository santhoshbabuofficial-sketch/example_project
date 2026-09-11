#include "current_sensor.hpp"                                      // Includes CurrentSensor class definition.

#include <zephyr/drivers/adc.h>                                    // Includes Zephyr ADC read functions.
#include <zephyr/logging/log.h>                                    // Includes Zephyr logging functions.


LOG_MODULE_REGISTER(current_sensor, LOG_LEVEL_INF);                // Creates log module for current sensor.


namespace motor_control {                                     // Opens motor control namespace.


CurrentSensor::CurrentSensor(GpioOverlay& overlay) : overlay_(overlay) {}
// Constructor connects current sensor object with GPIO/ADC hardware control.


bool CurrentSensor::Init() {                                       // Initializes current sensor ADC path.

    if (!device_is_ready(overlay_.current_adc_dev())) {            // Checks whether ADC1 device is ready.

        LOG_ERR("Current sensor ADC device not ready");            // Prints ADC error message.

        initialized_ = false;                                      // Marks sensor initialization as failed.

        return false;                                               // Returns failure.
    }

    initialized_ = true;                                            // Marks current sensor as initialized.

    return true;                                                    // Returns successful initialization.
}


bool CurrentSensor::Read(float& current_a_out) {                   // Reads ACS712 current value.

    if (!initialized_) {                                            // Checks whether sensor was initialized.

        return false;                                               // Returns failure if sensor is not ready.
    }

    int16_t raw_sample = 0;                                         // Creates buffer to hold raw ADC conversion result.

    adc_sequence sequence = {};                                     // Creates ADC read sequence descriptor.
    sequence.channels = BIT(GpioOverlay::kCurrentAdcChannel);       // Selects the ACS712 ADC channel.
    sequence.buffer = &raw_sample;                                   // Points sequence at the raw sample buffer.
    sequence.buffer_size = sizeof(raw_sample);                       // Sets buffer size in bytes.
    sequence.resolution = GpioOverlay::kAdcResolutionBits;           // Sets ADC resolution to 12 bits.

    const int err = adc_read(overlay_.current_adc_dev(), &sequence); // Performs the ADC conversion.

    if (err != 0) {                                                  // Checks if ADC read failed.

        LOG_WRN("Current sensor ADC read failed: %d", err);          // Prints ADC read error.

        return false;                                                // Returns failure.
    }

    const auto raw_code = static_cast<uint32_t>(raw_sample < 0 ? 0 : raw_sample);
    // Clamps a negative sample (should not normally occur) to zero.

    const float adc_pin_voltage =
        (static_cast<float>(raw_code) * kAdcRefVoltage) / static_cast<float>(kAdcMaxCode);
    // Converts raw ADC code into the voltage actually present on the ADC pin.

    const float acs_output_voltage = adc_pin_voltage / kAdcInputDividerRatio;
    // Recovers the ACS712 output voltage before any external divider.

    current_a_out = (acs_output_voltage - kAcsZeroCurrentVoltage) / kAcsSensitivityVPerA;
    // Converts ACS712 output voltage into current in amperes.

    return true;                                                     // Returns successful sensor reading.
}


}  // namespace motor_control

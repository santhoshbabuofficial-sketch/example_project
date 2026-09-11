#include "voltage_sensor.hpp"                                    // Includes VoltageSensor class definition.

#include <zephyr/drivers/adc.h>                                  // Includes Zephyr ADC read functions.
#include <zephyr/logging/log.h>                                  // Includes Zephyr logging functions.


LOG_MODULE_REGISTER(voltage_sensor, LOG_LEVEL_INF);              // Creates log module for voltage sensor.


namespace mc::motor_control {                                    // Opens motor control namespace.


VoltageSensor::VoltageSensor(GpioOverlay& overlay) : overlay_(overlay) {}
// Constructor connects voltage sensor object with GPIO/ADC hardware control.


bool VoltageSensor::Init() {                                       // Initializes voltage sensor ADC path.

    if (!device_is_ready(overlay_.voltage_adc_dev())) {            // Checks whether ADC2 device is ready.

        LOG_ERR("Voltage sensor ADC device not ready");            // Prints ADC error message.

        initialized_ = false;                                       // Marks sensor initialization as failed.

        return false;                                                // Returns failure.
    }

    initialized_ = true;                                            // Marks voltage sensor as initialized.

    return true;                                                    // Returns successful initialization.
}


bool VoltageSensor::Read(float& voltage_v_out) {                    // Reads voltage sensor module value.

    if (!initialized_) {                                             // Checks whether sensor was initialized.

        return false;                                                 // Returns failure if sensor is not ready.
    }

    int16_t raw_sample = 0;                                          // Creates buffer to hold raw ADC conversion result.

    adc_sequence sequence = {};                                      // Creates ADC read sequence descriptor.
    sequence.channels = BIT(GpioOverlay::kVoltageAdcChannel);        // Selects the voltage-sensor ADC channel.
    sequence.buffer = &raw_sample;                                    // Points sequence at the raw sample buffer.
    sequence.buffer_size = sizeof(raw_sample);                        // Sets buffer size in bytes.
    sequence.resolution = GpioOverlay::kAdcResolutionBits;            // Sets ADC resolution to 12 bits.

    const int err = adc_read(overlay_.voltage_adc_dev(), &sequence);  // Performs the ADC conversion.

    if (err != 0) {                                                    // Checks if ADC read failed.

        LOG_WRN("Voltage sensor ADC read failed: %d", err);            // Prints ADC read error.

        return false;                                                  // Returns failure.
    }

    const auto raw_code = static_cast<uint32_t>(raw_sample < 0 ? 0 : raw_sample);
    // Clamps a negative sample (should not normally occur) to zero.

    const float adc_pin_voltage =
        (static_cast<float>(raw_code) * kAdcRefVoltage) / static_cast<float>(kAdcMaxCode);
    // Converts raw ADC code into the voltage actually present on the ADC pin.

    voltage_v_out = adc_pin_voltage * kDividerRatio;
    // Scales the ADC pin voltage back up to the real supply/bus voltage.

    LOG_INF("Voltage sensor raw=%u Vpin=%d.%02u V Vbus=%d.%02u V", raw_code,
            static_cast<int>(adc_pin_voltage),
            static_cast<unsigned>((adc_pin_voltage - static_cast<int>(adc_pin_voltage)) * 100),
            static_cast<int>(voltage_v_out),
            static_cast<unsigned>((voltage_v_out - static_cast<int>(voltage_v_out)) * 100));
    // Prints raw code and computed bus voltage for debugging.

    return true;                                                       // Returns successful voltage reading.
}


}  // namespace mc::motor_control                                    // Closes motor control namespace.

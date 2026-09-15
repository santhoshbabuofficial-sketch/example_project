#include "voltage_sensor.hpp"                                    // Includes VoltageSensor / VoltageMonitor definitions.

#include <zephyr/devicetree.h>                                   // Includes Zephyr device tree support.
#include <zephyr/drivers/adc.h>                                  // Includes Zephyr ADC read functions.
#include <zephyr/logging/log.h>                                  // Includes Zephyr logging functions.


LOG_MODULE_REGISTER(voltage_sensor, LOG_LEVEL_INF);              // Creates log module for voltage sensor.


namespace motor_control {                                    // Opens motor control namespace.


// ===========================================================================
// VoltageSensor - ADC acquisition and conversion to real bus volts
// ===========================================================================

bool VoltageSensor::Init() {                                       // Initializes voltage sensor ADC path.

    adc_dev_ = DEVICE_DT_GET(DT_ALIAS(voltagesensor));             // Gets ADC2 device (0-25V voltage sensor module).

    if (!device_is_ready(adc_dev_)) {                              // Checks whether ADC2 device is ready.

        LOG_ERR("Voltage sensor ADC device not ready");            // Prints ADC error message.

        initialized_ = false;                                       // Marks sensor initialization as failed.

        return false;                                               // Returns failure.
    }

    const adc_channel_cfg voltage_cfg = {
        .gain = ADC_GAIN_1,                                         // Uses unity gain (no internal amplification).
        .reference = ADC_REF_INTERNAL,                              // Uses internal ADC reference voltage.
        .acquisition_time = ADC_ACQ_TIME_DEFAULT,                    // Uses driver default acquisition time.
        .channel_id = kVoltageAdcChannel,                            // Selects ADC2_IN17 (PA4).
        .differential = 0,                                           // Uses single-ended measurement.
    };

    if (adc_channel_setup(adc_dev_, &voltage_cfg) != 0) {           // Configures the voltage sensing ADC channel.

        LOG_ERR("Voltage sensor ADC channel setup failed");         // Prints ADC channel configuration error.

        initialized_ = false;                                        // Marks sensor initialization as failed.

        return false;                                                // Returns failure.
    }

    initialized_ = true;                                             // Marks voltage sensor as initialized.

    return true;                                                     // Returns successful initialization.
}


bool VoltageSensor::Read(float& voltage_v_out) {                    // Reads voltage sensor module value.

    if (!initialized_) {                                             // Checks whether sensor was initialized.

        return false;                                                 // Returns failure if sensor is not ready.
    }

    uint32_t accumulated_code = 0U;                                   // Accumulates raw codes across the averaged samples.

    for (uint8_t i = 0U; i < kAverageSamples; ++i) {                  // Takes several conversions to average out noise.

        int16_t raw_sample = 0;                                       // Creates buffer to hold raw ADC conversion result.

        adc_sequence sequence = {};                                   // Creates ADC read sequence descriptor.
        sequence.channels = BIT(kVoltageAdcChannel);                  // Selects the voltage-sensor ADC channel.
        sequence.buffer = &raw_sample;                                 // Points sequence at the raw sample buffer.
        sequence.buffer_size = sizeof(raw_sample);                     // Sets buffer size in bytes.
        sequence.resolution = kAdcResolutionBits;                      // Sets ADC resolution to 12 bits.

        const int err = adc_read(adc_dev_, &sequence);                 // Performs the ADC conversion.

        if (err != 0) {                                                // Checks if ADC read failed.

            LOG_WRN("Voltage sensor ADC read failed: %d", err);        // Prints ADC read error.

            return false;                                              // Returns failure - caller treats this as a fault.
        }

        accumulated_code += static_cast<uint32_t>(raw_sample < 0 ? 0 : raw_sample);
        // Clamps a negative sample (should not normally occur) to zero, then accumulates.
    }

    const auto raw_code = accumulated_code / kAverageSamples;          // Averages the accumulated raw ADC codes.

    const float adc_pin_voltage =
        (static_cast<float>(raw_code) * kAdcRefVoltage) / static_cast<float>(kAdcMaxCode);
    // Converts averaged raw ADC code into the voltage actually present on the ADC pin.

    voltage_v_out = adc_pin_voltage * kDividerRatio;
    // Scales the ADC pin voltage back up to the real supply/bus voltage.

    LOG_DBG("Voltage sensor raw=%u Vbus=%d.%02u V", raw_code,
            static_cast<int>(voltage_v_out),
            static_cast<unsigned>((voltage_v_out - static_cast<int>(voltage_v_out)) * 100));
    // Prints raw code and computed bus voltage for debugging.

    return true;                                                       // Returns successful voltage reading.
}


// ===========================================================================
// VoltageMonitor - window supervision and mutually-exclusive trigger pins
// ===========================================================================

VoltageMonitor::VoltageMonitor(IVoltageSensor& sensor) : sensor_(sensor) {}
// Constructor connects the supervisor with the voltage sensor it evaluates.


bool VoltageMonitor::Init() {                                         // Initializes trigger pins and underlying sensor.

    if (!sensor_.Init()) {                                             // Initializes the ADC voltage sensing path.

        LOG_ERR("Voltage monitor: sensor init failed");                // Prints sensor initialization error.

        return false;                                                  // Returns failure.
    }

    normal_trigger_ = GPIO_DT_SPEC_GET(DT_ALIAS(normaltrigger), gpios); // Resolves normal-state trigger pin spec (PC0).
    fault_trigger_  = GPIO_DT_SPEC_GET(DT_ALIAS(faulttrigger), gpios);  // Resolves fault-state trigger pin spec (PC1).

    if (!gpio_is_ready_dt(&normal_trigger_) || !gpio_is_ready_dt(&fault_trigger_)) {

        LOG_ERR("Voltage monitor: trigger pin(s) not ready");          // Prints trigger pin error.

        return false;                                                  // Returns failure.
    }

    // Both pins start LOW. Nothing is asserted until a valid reading has
    // been classified, so a sensor that never comes up leaves both
    // outputs inactive rather than falsely asserting "normal".
    if (gpio_pin_configure_dt(&normal_trigger_, GPIO_OUTPUT_INACTIVE) != 0 ||
        gpio_pin_configure_dt(&fault_trigger_, GPIO_OUTPUT_INACTIVE) != 0) {

        LOG_ERR("Voltage monitor: trigger pin configure failed");      // Prints pin configuration error.

        return false;                                                  // Returns failure.
    }

    state_ = VoltageState::kUnknown;                                    // Starts in the unknown state with both pins LOW.

    initialized_ = true;                                                // Marks supervisor initialization as completed.

    LOG_INF("Voltage monitor ready: nominal %d V, window %d.%02u - %d.%02u V",
            static_cast<int>(kNominalVoltage),
            static_cast<int>(kUnderVoltageLimit),
            static_cast<unsigned>((kUnderVoltageLimit - static_cast<int>(kUnderVoltageLimit)) * 100),
            static_cast<int>(kOverVoltageLimit),
            static_cast<unsigned>((kOverVoltageLimit - static_cast<int>(kOverVoltageLimit)) * 100));
    // Prints the supervision window actually compiled in.

    return true;                                                        // Returns successful initialization.
}


void VoltageMonitor::ApplyState(VoltageState new_state) {               // Drives the trigger pins for the given state.

    if (new_state == state_) {                                          // Checks whether the state actually changed.

        return;                                                         // Skips redundant pin writes.
    }

    // BREAK-BEFORE-MAKE: the outgoing pin is always driven LOW first, so
    // there is no instant at which both trigger pins are HIGH.
    switch (new_state) {

    case VoltageState::kNormal:

        gpio_pin_set_dt(&fault_trigger_, 0);                            // Drives fault trigger PC1 LOW first.
        gpio_pin_set_dt(&normal_trigger_, 1);                           // Drives normal trigger PC0 HIGH.

        LOG_INF("Voltage NORMAL -> PC0 HIGH, PC1 LOW");                 // Prints the asserted normal state.
        break;

    case VoltageState::kFault:

        gpio_pin_set_dt(&normal_trigger_, 0);                           // Drives normal trigger PC0 LOW first.
        gpio_pin_set_dt(&fault_trigger_, 1);                            // Drives fault trigger PC1 HIGH.

        LOG_WRN("Voltage FAULT -> PC1 HIGH, PC0 LOW");                  // Prints the asserted fault state.
        break;

    case VoltageState::kUnknown:
    default:

        gpio_pin_set_dt(&normal_trigger_, 0);                           // Drives normal trigger PC0 LOW.
        gpio_pin_set_dt(&fault_trigger_, 0);                            // Drives fault trigger PC1 LOW.

        LOG_WRN("Voltage UNKNOWN -> both triggers LOW");                // Prints the inactive state.
        break;
    }

    state_ = new_state;                                                  // Stores the newly asserted state.
}


VoltageState VoltageMonitor::Update() {                                  // Reads, classifies, and drives the triggers.

    if (!initialized_) {                                                 // Checks whether the supervisor was initialized.

        return VoltageState::kUnknown;                                   // Returns unknown if not ready.
    }

    float bus_voltage = 0.0F;                                            // Holds the measured bus voltage in volts.

    if (!sensor_.Read(bus_voltage)) {                                    // Attempts to read the bus voltage.

        // A sensor that cannot be read is treated as a fault, not as
        // "normal" - failing safe rather than failing silent.
        LOG_ERR("Voltage read failed - asserting fault");                // Prints the read failure.

        ApplyState(VoltageState::kFault);                                // Asserts the fault trigger.

        return state_;                                                   // Returns the asserted state.
    }

    // Threshold selection depends on the current state (hysteresis):
    // a healthy supply may drift right up to a limit, so returning to
    // normal requires coming back a margin inside the window.
    float upper_limit = kOverVoltageLimit;                               // Selects the applicable upper trip limit.
    float lower_limit = kUnderVoltageLimit;                              // Selects the applicable lower trip limit.

    if (state_ != VoltageState::kNormal) {                               // Checks whether recovering into the normal window.

        upper_limit = kOverVoltageLimit - kRecoveryHysteresis;           // Tightens the upper limit for recovery.
        lower_limit = kUnderVoltageLimit + kRecoveryHysteresis;          // Tightens the lower limit for recovery.
    }

    if ((bus_voltage > upper_limit) || (bus_voltage < lower_limit)) {    // Checks for over-voltage or under-voltage.

        LOG_WRN("Voltage out of range: %d.%02u V",
                static_cast<int>(bus_voltage),
                static_cast<unsigned>((bus_voltage - static_cast<int>(bus_voltage)) * 100));
        // Prints the offending measurement.

        ApplyState(VoltageState::kFault);                                 // Asserts the fault trigger.

    } else {

        ApplyState(VoltageState::kNormal);                                // Asserts the normal trigger.
    }

    return state_;                                                        // Returns the asserted state.
}


}  // namespace motor_control

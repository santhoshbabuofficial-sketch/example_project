#pragma once

#include <cstdint>                                        // Includes fixed-size integer types.

#include <zephyr/device.h>                                // Includes Zephyr device structure and APIs.
#include <zephyr/drivers/gpio.h>                          // Includes Zephyr GPIO driver functions.


namespace motor_control {                              // Opens motor control namespace.


// Voltage supervision result. Exactly one of these is active at any time,
// which is what guarantees PC0 and PC1 can never be driven high together.
enum class VoltageState : uint8_t {                       // Creates enum describing supervised voltage condition.

    kUnknown = 0U,                                        // No valid reading taken yet - both trigger pins stay LOW.

    kNormal  = 1U,                                        // Voltage inside the accepted window -> PC0 HIGH, PC1 LOW.

    kFault   = 2U,                                        // Voltage over/under limit or unreadable -> PC1 HIGH, PC0 LOW.
};


class IVoltageSensor {                                    // Creates interface class for voltage sensor abstraction.

public:

    virtual ~IVoltageSensor() = default;                  // Virtual destructor allows safe object deletion.

    virtual bool Init() = 0;                              // Pure virtual function for sensor initialization.

    virtual bool Read(float& voltage_v_out) = 0;          // Pure virtual function to read voltage value in volts.
};


// Generic "0-25V DC voltage sensor module" - a fixed resistor divider
// (commonly 30k / 7.5k, ratio 5:1) feeding STM32G4 ADC2 channel on PA4.
class VoltageSensor final : public IVoltageSensor {       // Creates VoltageSensor class implementing IVoltageSensor.

public:

    VoltageSensor() = default;                            // Creates default constructor for VoltageSensor object.

    bool Init() override;                                 // Initializes voltage sensor ADC reading path.

    bool Read(float& voltage_v_out) override;             // Reads ADC and converts to volts.


    static constexpr uint8_t kVoltageAdcChannel = 17U;    // ADC2_IN17 -> PA4 -> voltage divider output.

    static constexpr uint8_t kAdcResolutionBits = 12U;    // STM32G4 ADC resolution used for the voltage channel.


private:

    static constexpr float kDividerRatio = 5.0F;
    // 0-25V module divider ratio: Vin = Vadc * 5 (verify against your
    // specific module if it differs).

    static constexpr float kAdcRefVoltage = 3.3F;         // STM32G4 ADC reference voltage.

    static constexpr uint32_t kAdcMaxCode =
        (1U << kAdcResolutionBits) - 1U;                   // Maximum raw ADC code (4095 for 12-bit).

    static constexpr uint8_t kAverageSamples = 8U;
    // Averages several conversions per Read() so ADC/divider noise does
    // not bounce the supervisor across a threshold on a single bad sample.

    const device* adc_dev_ = nullptr;                      // Stores ADC2 device pointer (voltage sensor).

    bool initialized_ = false;                             // Stores whether voltage sensor initialization is completed.
};


// Supervises the measured bus voltage against the nominal 24 V window and
// drives the two trigger outputs.
//
// MUTUAL EXCLUSION: both pins are written only from ApplyState(), which
// always drives the outgoing pin LOW *before* driving the incoming pin
// HIGH (break-before-make). There is no other code path that touches
// either pin, so PC0 and PC1 can never be HIGH simultaneously - not at
// boot, not during a transition, and not on a fault.
class VoltageMonitor {                                     // Creates class that maps voltage readings onto trigger pins.

public:

    explicit VoltageMonitor(IVoltageSensor& sensor);        // Constructor connects supervisor with a voltage sensor.

    bool Init();                                            // Initializes trigger pins and the underlying sensor.

    // Takes one reading, evaluates it against the window, and updates the
    // trigger pins. Returns the state now being asserted.
    VoltageState Update();

    VoltageState state() const { return state_; }           // Returns the currently asserted supervision state.


    // ---- Nominal 24 V supervision window -----------------------------------

    static constexpr float kNominalVoltage = 24.0F;         // Nominal bus voltage of the supervised supply.

    static constexpr float kOverVoltageLimit  = 26.4F;      // Trip above this (+10% of nominal).

    static constexpr float kUnderVoltageLimit = 21.6F;      // Trip below this (-10% of nominal).

    static constexpr float kRecoveryHysteresis = 0.5F;
    // Once faulted, the voltage must come back 0.5 V *inside* the trip
    // limits before returning to normal. Without this, a supply sitting
    // exactly on a limit would chatter both relays/pins continuously.


private:

    void ApplyState(VoltageState new_state);                 // Drives trigger pins for a state, break-before-make.

    IVoltageSensor& sensor_;                                  // Stores reference to the voltage sensor being supervised.

    gpio_dt_spec normal_trigger_{};                            // Stores normal-state trigger pin spec (PC0).

    gpio_dt_spec fault_trigger_{};                             // Stores fault-state trigger pin spec (PC1).

    VoltageState state_ = VoltageState::kUnknown;              // Stores currently asserted supervision state.

    bool initialized_ = false;                                 // Stores whether supervisor initialization is completed.
};


}  // namespace motor_control

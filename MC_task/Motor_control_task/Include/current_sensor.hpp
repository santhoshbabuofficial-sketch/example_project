#pragma once                                              // Prevents this header file from being included multiple times.

#include <cstdint>                                        // Includes fixed-size integer types.

#include "gpio_overlay.hpp"                                // Includes GPIO/ADC hardware access.


namespace mc::motor_control {                             // Opens motor control namespace.


class ICurrentSensor {                                     // Creates interface class for current sensor abstraction.

public:

    virtual ~ICurrentSensor() = default;                    // Virtual destructor allows safe object deletion.

    virtual bool Init() = 0;                                // Pure virtual function for sensor initialization.

    virtual bool Read(float& current_a_out) = 0;            // Pure virtual function to read current value in amperes.
};


// ACS712 hall-effect current sensor, read through the STM32G4 ADC1 channel
// wired to PA0. Sensitivity/zero-offset are named constants below - verify
// against your actual ACS712 variant (5A/20A/30A) and adjust if needed.
class CurrentSensor final : public ICurrentSensor {         // Creates CurrentSensor class implementing ICurrentSensor.

public:

    explicit CurrentSensor(GpioOverlay& overlay);           // Constructor connects sensor with GPIO/ADC hardware.

    bool Init() override;                                   // Initializes current sensor ADC reading path.

    bool Read(float& current_a_out) override;                // Reads ADC and converts to amperes.


private:

    // ---- ACS712 electrical characteristics (VERIFY against your module) --

    static constexpr float kAcsSupplyVoltage = 5.0F;         // ACS712 module supply voltage (typical 5.0V).

    static constexpr float kAcsZeroCurrentVoltage = kAcsSupplyVoltage / 2.0F;
    // Output voltage at 0A is nominally Vcc/2 per ACS712 datasheet.

    static constexpr float kAcsSensitivityVPerA = 0.185F;
    // ACS712-05B sensitivity is 185 mV/A. Change to 0.100F for the 20A
    // variant or 0.066F for the 30A variant if that is what is fitted.

    static constexpr float kAdcInputDividerRatio = 1.0F;
    // Ratio applied between the ACS712 output and the ADC pin, if any
    // external divider/level-shift is present on the board. 1.0 assumes
    // the ACS712 output is fed to the ADC pin directly/already conditioned
    // into the 0-3.3V range - update this if a divider is present.

    // ---- STM32G4 ADC characteristics --------------------------------------

    static constexpr float kAdcRefVoltage = 3.3F;             // STM32G4 ADC reference voltage.

    static constexpr uint32_t kAdcMaxCode =
        (1U << GpioOverlay::kAdcResolutionBits) - 1U;          // Maximum raw ADC code (4095 for 12-bit).

    GpioOverlay& overlay_;                                    // Stores reference to GPIO/ADC hardware control object.

    bool initialized_ = false;                                // Stores sensor initialization status.
};


}  // namespace mc::motor_control                            // Closes motor control namespace.

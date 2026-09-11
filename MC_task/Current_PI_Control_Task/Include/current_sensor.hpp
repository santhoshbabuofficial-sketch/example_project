#pragma once

#include <cstdint>                                        // Includes fixed-size integer types.

#include "gpio_overlay.hpp"


namespace motor_control {                            // Opens motor control namespace.


class ICurrentSensor {                                     // Creates interface class for current sensor abstraction.

public:

    virtual ~ICurrentSensor() = default;                    // Virtual destructor allows safe object deletion.

    virtual bool Init() = 0;                                // Pure virtual function for sensor initialization.

    virtual bool Read(float& current_a_out) = 0;            // Pure virtual function to read current value in amperes.
};


class CurrentSensor final : public ICurrentSensor {         // Creates CurrentSensor class implementing ICurrentSensor.

public:

    explicit CurrentSensor(GpioOverlay& overlay);           // Constructor connects sensor with GPIO/ADC hardware.

    bool Init() override;                                   // Initializes current sensor ADC reading path.

    bool Read(float& current_a_out) override;                // Reads ADC and converts to amperes.


private:

    static constexpr float kAcsSupplyVoltage = 5.0F;         // ACS712 module supply voltage (typical 5.0V).

    static constexpr float kAcsZeroCurrentVoltage = kAcsSupplyVoltage / 2.0F;
    // Output voltage at 0A is nominally Vcc/2 per ACS712 datasheet.

    static constexpr float kAcsSensitivityVPerA = 0.185F;
    // ACS712-05B sensitivity is 185 mV/A. Change to 0.100F for the 20A
    // variant or 0.066F for the 30A variant if that is what is fitted.
    // This scale factor directly sets how "loud" a given coil current
    // reads to the PI loop - re-check it first if tracking looks off.

    static constexpr float kAdcInputDividerRatio = 1.0F;

    // ---- STM32G4 ADC characteristics ------

    static constexpr float kAdcRefVoltage = 3.3F;             // STM32G4 ADC reference voltage.

    static constexpr uint32_t kAdcMaxCode =
        (1U << GpioOverlay::kAdcResolutionBits) - 1U;          // Maximum raw ADC code (4095 for 12-bit).

    GpioOverlay& overlay_;                                    // Stores reference to GPIO/ADC hardware control object.

    bool initialized_ = false;                                // Stores sensor initialization status.
};


}  // namespace motor_control

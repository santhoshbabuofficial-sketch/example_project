#pragma once                                             

#include <cstdint>                                        // Includes fixed-size integer types.

#include "gpio_overlay.hpp"                               


namespace motor_control {                              // Opens motor control namespace.


class IVoltageSensor {                                      // Creates interface class for voltage sensor abstraction.

public:

    virtual ~IVoltageSensor() = default;                     // Virtual destructor allows safe object deletion.

    virtual bool Init() = 0;                                 // Pure virtual function for sensor initialization.

    virtual bool Read(float& voltage_v_out) = 0;             // Pure virtual function to read voltage value in volts.
};


// Generic "0-25V DC voltage sensor module" - a fixed resistor divider
// (commonly 30k / 7.5k, ratio 5:1) feeding STM32G4 ADC2 channel on PA4.
class VoltageSensor final : public IVoltageSensor {          // Creates VoltageSensor class implementing IVoltageSensor.

public:

    explicit VoltageSensor(GpioOverlay& overlay);            // Constructor connects sensor with GPIO/ADC hardware.

    bool Init() override;                                    // Initializes voltage sensor ADC reading path.

    bool Read(float& voltage_v_out) override;                 // Reads ADC and converts to volts.


private:

    static constexpr float kDividerRatio = 5.0F;
    // 0-25V module divider ratio: Vin = Vadc * 5 (verify against your
    // specific module if it differs).

    static constexpr float kAdcRefVoltage = 3.3F;              // STM32G4 ADC reference voltage.

    static constexpr uint32_t kAdcMaxCode =
        (1U << GpioOverlay::kAdcResolutionBits) - 1U;           // Maximum raw ADC code (4095 for 12-bit).

    GpioOverlay& overlay_;                                     // Stores reference to GPIO/ADC hardware control object.

    bool initialized_ = false;                                 // Stores whether voltage sensor initialization is completed.
};


}  // namespace mc::motor_control                             

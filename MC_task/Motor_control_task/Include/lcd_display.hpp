#pragma once                                              // Prevents this header file from being included multiple times.

#include <cstdint>                                        // Includes fixed-size integer types like uint8_t.

#include "gpio_overlay.hpp"                                // Includes GPIO and I2C hardware control functions.


namespace mc::motor_control {                              // Opens motor control namespace.


class ILcdDisplay {                                        // Creates LCD interface class for abstraction.

public:

    virtual ~ILcdDisplay() = default;
    // Creates virtual destructor for safe deletion of derived LCD objects.

    virtual bool Init() = 0;
    // Defines function to initialize LCD hardware, implemented by child class.

    virtual void ShowVoltageCurrent(float voltage_v, float current_a) = 0;
    // Displays voltage and current together on LCD row 0.

    virtual void ShowPwmStatus(uint8_t pwm_percent, bool forward) = 0;
    // Displays commanded PWM duty and direction on LCD row 1.

    virtual void ShowFault(const char* reason_text) = 0;
    // Displays a fault message across both LCD rows.
};


class LcdDisplay final : public ILcdDisplay {              // Creates LCD class that implements ILcdDisplay interface.

public:

    explicit LcdDisplay(GpioOverlay& overlay);
    // Constructor connects LCD object with GPIO/I2C hardware control.

    bool Init() override;
    // Initializes LCD hardware communication.

    void ShowVoltageCurrent(float voltage_v, float current_a) override;
    // Displays voltage (V) and current (A) together on row 0.

    void ShowPwmStatus(uint8_t pwm_percent, bool forward) override;
    // Displays PWM duty percentage and direction on row 1.

    void ShowFault(const char* reason_text) override;
    // Displays a fault message across both rows.


private:

    static constexpr uint8_t kLcdColumns = 16U;
    // Defines LCD number of columns (16 characters).

    static constexpr uint8_t kLcdRows = 2U;
    // Defines LCD number of rows (2 lines).

    static constexpr uint8_t kBacklightBit = 0x08U;
    // Defines bit position used to control LCD backlight.

    static constexpr uint8_t kEnableBit = 0x04U;
    // Defines bit position used for LCD enable signal.

    static constexpr uint8_t kRegisterSelectBit = 0x01U;
    // Defines bit position used to select LCD command/data mode.

    void WriteNibble(uint8_t nibble, bool is_data);
    // Sends 4-bit data to LCD through I2C interface.

    void WriteByte(uint8_t value, bool is_data);
    // Sends complete 8-bit data or command to LCD.

    void SetCursor(uint8_t row, uint8_t col);
    // Moves LCD cursor position to given row and column.

    void WriteLine(uint8_t row, const char* text);
    // Writes text data on selected LCD row.


    GpioOverlay& overlay_;
    // Stores reference to GPIO object for LCD communication.

    bool initialized_ = false;
    // Stores whether LCD initialization is completed.

    bool i2c_error_logged_ = false;
    // Prevents repeated logging of same I2C communication error.
};


}  // namespace mc::motor_control                         // Closes motor control namespace.

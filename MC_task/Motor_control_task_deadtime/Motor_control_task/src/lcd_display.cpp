#include "lcd_display.hpp"                                      // Includes LCD display class definition.

#include <cstdio>                                                // Includes snprintf function for formatting text.

#include <zephyr/drivers/i2c.h>                                  // Includes Zephyr I2C communication functions.

#include <zephyr/kernel.h>                                       // Includes Zephyr kernel functions like delay.

#include <zephyr/logging/log.h>                                  // Includes Zephyr logging functions.


LOG_MODULE_REGISTER(lcd_display, LOG_LEVEL_INF);                 // Creates log module for LCD display.


namespace motor_control {                                    // Opens motor control namespace.


namespace {                                                      // Opens private namespace for local constants.

constexpr uint8_t kCmdClearDisplay = 0x01U;                      // Defines LCD command to clear display.

constexpr uint8_t kCmdFunctionSet4Bit2Line = 0x28U;              // Defines LCD mode as 4-bit, 2-line operation.

constexpr uint8_t kCmdDisplayOn = 0x0CU;                         // Defines LCD display ON command.

constexpr uint8_t kCmdEntryModeIncrement = 0x06U;                // Defines cursor increment mode.

constexpr uint8_t kRowZeroAddr = 0x80U;                          // Defines LCD first row starting address.

constexpr uint8_t kRowOneAddr = 0xC0U;                           // Defines LCD second row starting address.

}  // namespace                                                // Closes private namespace.


LcdDisplay::LcdDisplay(GpioOverlay& overlay) : overlay_(overlay) {}
// Constructor connects LCD object with GPIO/I2C hardware control.


void LcdDisplay::WriteNibble(uint8_t nibble, bool is_data) {
// Sends 4-bit data to LCD through I2C using PCF8574.

    uint8_t frame = static_cast<uint8_t>((nibble & 0xF0U) | kBacklightBit);
    // Creates I2C frame with LCD data bits and keeps backlight ON.

    if (is_data) {                                               // Checks whether data or command is being sent.

        frame = static_cast<uint8_t>(frame | kRegisterSelectBit);
        // Sets RS bit for sending display data.
    }

    const uint8_t enable_high = static_cast<uint8_t>(frame | kEnableBit);
    // Creates frame with LCD enable signal HIGH.

    const int err_high =
        i2c_write(overlay_.lcd_i2c_bus(), &enable_high, 1U, GpioOverlay::kLcdI2cAddr);
    // Sends enable HIGH signal to LCD through I2C.

    k_busy_wait(1);                                              // Waits small delay for LCD timing.

    const uint8_t enable_low = static_cast<uint8_t>(frame & ~kEnableBit);
    // Creates frame with LCD enable signal LOW.

    const int err_low =
        i2c_write(overlay_.lcd_i2c_bus(), &enable_low, 1U, GpioOverlay::kLcdI2cAddr);
    // Sends enable LOW signal to complete LCD write.

    k_busy_wait(50);                                             // Waits for LCD processing time.

    if ((err_high != 0 || err_low != 0) && !i2c_error_logged_) {
    // Checks I2C error and logs only first failure.

        LOG_ERR("LCD PCF8574 I2C write failed (addr=0x%02x, err=%d/%d) - check wiring/power",
                GpioOverlay::kLcdI2cAddr, err_high, err_low);
        // Prints LCD communication error.

        i2c_error_logged_ = true;                                // Prevents repeated error messages.
    }
}


void LcdDisplay::WriteByte(uint8_t value, bool is_data) {
// Sends complete 8-bit command/data to LCD using two 4-bit transfers.

    WriteNibble(static_cast<uint8_t>(value & 0xF0U), is_data);
    // Sends upper 4 bits of data.

    WriteNibble(static_cast<uint8_t>((value << 4U) & 0xF0U), is_data);
    // Sends lower 4 bits of data.
}


bool LcdDisplay::Init() {                                       // Initializes LCD module.

    k_mutex_init(&lock_);                                        // Prepares the mutex guarding shared I2C/LCD access.

    k_msleep(50);                                                // Waits for LCD power stabilization.

    WriteNibble(0x30U, false);                                   // Sends LCD initialization command.

    k_msleep(5);                                                 // Waits for LCD response.

    WriteNibble(0x30U, false);                                   // Sends second initialization command.

    k_busy_wait(150);                                            // Provides microsecond delay.

    WriteNibble(0x30U, false);                                   // Sends third initialization command.

    WriteNibble(0x20U, false);                                   // Changes LCD communication to 4-bit mode.

    WriteByte(kCmdFunctionSet4Bit2Line, false);                 // Configures LCD as 4-bit 2-line display.

    WriteByte(kCmdDisplayOn, false);                            // Turns LCD display ON.

    WriteByte(kCmdClearDisplay, false);                          // Clears LCD screen.

    k_msleep(2);                                                 // Waits for clear command completion.

    WriteByte(kCmdEntryModeIncrement, false);                   // Sets cursor movement direction.

    initialized_ = true;                                         // Stores LCD initialization success.

    return true;                                                 // Returns successful initialization.
}


void LcdDisplay::SetCursor(uint8_t row, uint8_t col) {
// Moves LCD cursor to selected row and column.

    const uint8_t base = (row == 0U) ? kRowZeroAddr : kRowOneAddr;
    // Selects LCD row starting address.

    WriteByte(static_cast<uint8_t>(base + col), false);
    // Sends cursor position command.
}


void LcdDisplay::WriteLine(uint8_t row, const char* text) {
// Writes text data to selected LCD row, space-padded to clear leftovers.

    SetCursor(row, 0U);                                          // Moves cursor to beginning of row.

    for (uint8_t i = 0U; i < kLcdColumns; ++i) {
    // Loops through all LCD columns, padding with spaces past the string end.

        const char c = text[i];                                  // Reads one character from text.

        WriteByte(static_cast<uint8_t>(c == '\0' ? ' ' : c), true);
        // Sends character data to LCD, or a blank to clear stale characters.
    }
}


void LcdDisplay::ShowVoltageCurrent(float voltage_v, float current_a) {
// Displays voltage and current together on LCD row 0.

    if (!initialized_) {                                         // Checks if LCD is initialized.

        return;                                                  // Stops if LCD is not ready.
    }

    k_mutex_lock(&lock_, K_FOREVER);                              // Locks LCD - this is called from the sensor thread while
                                                                   // the ramp thread may concurrently call ShowPwmStatus().

    char line[kLcdColumns + 1U] = {0};
    // Creates text buffer for LCD line.

    std::snprintf(line, sizeof(line), "V:%5.1f I:%4.2fA", static_cast<double>(voltage_v),
                  static_cast<double>(current_a));
    // Formats voltage and current into one 16-character display line.

    WriteLine(0U, line);                                         // Displays voltage/current on first LCD row.

    k_mutex_unlock(&lock_);                                       // Releases the LCD lock.
}


void LcdDisplay::ShowPwmStatus(uint8_t pwm_percent, bool forward) {
// Displays commanded PWM duty and direction on LCD row 1.

    if (!initialized_) {                                         // Checks if LCD is initialized.

        return;                                                  // Stops if LCD is not ready.
    }

    k_mutex_lock(&lock_, K_FOREVER);                              // Locks LCD - this is called from the ramp thread every
                                                                   // 300 ms while the sensor thread may concurrently call
                                                                   // ShowVoltageCurrent()/ShowFault().

    char line[kLcdColumns + 1U] = {0};
    // Creates text buffer for LCD line.

    std::snprintf(line, sizeof(line), "PWM:%3u%% DIR:%s", static_cast<unsigned>(pwm_percent),
                  forward ? "FWD" : "REV");
    // Formats PWM percentage and direction into one 16-character display line.

    WriteLine(1U, line);                                         // Displays PWM/direction on second LCD row.

    k_mutex_unlock(&lock_);                                       // Releases the LCD lock.
}


void LcdDisplay::ShowFault(const char* reason_text) {
// Displays a fault message across both LCD rows.

    if (!initialized_) {                                         // Checks if LCD is initialized.

        return;                                                  // Stops if LCD is not ready.
    }

    k_mutex_lock(&lock_, K_FOREVER);                              // Locks LCD - called from the sensor thread on fault.

    WriteLine(0U, "*** FAULT ***");                              // Shows fault banner on first row.

    WriteLine(1U, reason_text);                                    // Shows fault reason text on second row.

    k_mutex_unlock(&lock_);                                       // Releases the LCD lock.
}


}  // namespace mc::motor_control                              
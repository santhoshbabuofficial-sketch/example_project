#pragma once

#include <cstdint>

#include <zephyr/drivers/gpio.h>

namespace bldc {

// Reads three 120-degree-spaced hall sensor inputs and registers a
// combined edge interrupt for them.
//
// Simplification: assumes all three hall pins live on the same GPIO
// port, since gpio_add_callback() registers per-port. If your hall
// sensors are wired across multiple ports, register one gpio_callback
// per port instead of the single one used here.
class HallSensor {
public:
    struct HallHw {
        gpio_dt_spec u;
        gpio_dt_spec v;
        gpio_dt_spec w;
    };

    explicit HallSensor(const HallHw& hw);

    // handler/callback_data must outlive the HallSensor (pass a
    // statically-allocated gpio_callback, as in main.cpp).
    [[nodiscard]] bool Init(gpio_callback_handler_t handler, gpio_callback& callback_data);

    // Combines the three current pin states into a 3-bit code:
    // bit0 = U, bit1 = V, bit2 = W. Returns 0 (an invalid/fault code) if
    // any pin read fails.
    [[nodiscard]] uint8_t ReadCode() const;

private:
    HallHw hw_;
};

}  // namespace bldc

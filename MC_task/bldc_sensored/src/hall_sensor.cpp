#include "hall_sensor.hpp"

#include <array>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(bldc_hall_sensor, LOG_LEVEL_INF);

namespace bldc {

HallSensor::HallSensor(const HallHw& hw) : hw_(hw) {}

bool HallSensor::Init(gpio_callback_handler_t handler, gpio_callback& callback_data) {
    const std::array<const gpio_dt_spec*, 3U> pins{&hw_.u, &hw_.v, &hw_.w};

    for (const auto* pin : pins) {
        if (!gpio_is_ready_dt(pin)) {
            LOG_ERR("Hall GPIO not ready");
            return false;
        }
        if (gpio_pin_configure_dt(pin, GPIO_INPUT) != 0) {
            LOG_ERR("Failed to configure hall GPIO as input");
            return false;
        }
        if (gpio_pin_interrupt_configure_dt(pin, GPIO_INT_EDGE_BOTH) != 0) {
            LOG_ERR("Failed to configure hall GPIO interrupt");
            return false;
        }
    }

    gpio_init_callback(&callback_data, handler,
                        BIT(hw_.u.pin) | BIT(hw_.v.pin) | BIT(hw_.w.pin));

    if (gpio_add_callback(hw_.u.port, &callback_data) != 0) {
        LOG_ERR("Failed to add hall GPIO callback");
        return false;
    }

    return true;
}

uint8_t HallSensor::ReadCode() const {
    const int u = gpio_pin_get_dt(&hw_.u);
    const int v = gpio_pin_get_dt(&hw_.v);
    const int w = gpio_pin_get_dt(&hw_.w);

    if ((u < 0) || (v < 0) || (w < 0)) {
        return 0U;  // Invalid code - caller treats this as a fault.
    }

    return static_cast<uint8_t>((static_cast<uint8_t>(u) << 0U) |
                                 (static_cast<uint8_t>(v) << 1U) |
                                 (static_cast<uint8_t>(w) << 2U));
}

}  // namespace bldc

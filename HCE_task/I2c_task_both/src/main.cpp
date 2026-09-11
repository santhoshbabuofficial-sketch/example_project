#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>

LOG_MODULE_REGISTER(I2c_task, LOG_LEVEL_INF);

namespace {

const struct device* i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));

// TODO: replace with the actual 7-bit address of the target I2C device.
constexpr uint16_t kI2cSlaveAddr = 0x50U;

}  // namespace

int main() {

    LOG_INF("Board is booting up");

    if (!device_is_ready(i2c_dev))
    {
        LOG_ERR("I2C device is not ready");
        return -1;
    }

    LOG_INF("Board is running");

    uint8_t tx_byte = 0x00U;
    uint8_t rx_byte = 0x00U;

    while (true)
    {
        int ret = i2c_write_read(i2c_dev, kI2cSlaveAddr,
                                  &tx_byte, sizeof(tx_byte),
                                  &rx_byte, sizeof(rx_byte));

        if (ret == 0)
        {
            LOG_INF("Received byte: 0x%02x", rx_byte);
        }
        else
        {
            LOG_ERR("I2C transfer failed: %d", ret);
        }

        k_msleep(500);
    }

    return 0;
}

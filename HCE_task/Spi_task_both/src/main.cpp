#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>

LOG_MODULE_REGISTER(Spi_task, LOG_LEVEL_INF);

namespace {

const struct device* spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi1));

struct spi_cs_control cs_ctrl = {
    .gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(spi1), cs_gpios),
    .delay = 0U,
};

struct spi_config spi_cfg = {
    .frequency = 1000000U,
    .operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_OP_MODE_MASTER,
    .slave = 0U,
    .cs = cs_ctrl,
};

}  // namespace

int main() {

    LOG_INF("Board is booting up");

    if (!device_is_ready(spi_dev))
    {
        LOG_ERR("SPI device is not ready");
        return -1;
    }

    if (!device_is_ready(cs_ctrl.gpio.port))
    {
        LOG_ERR("SPI CS GPIO is not ready");
        return -1;
    }

    LOG_INF("Board is running");

    uint8_t tx_byte = 0x00U;
    uint8_t rx_byte = 0x00U;

    struct spi_buf tx_buf = { .buf = &tx_byte, .len = 1U };
    struct spi_buf_set tx_set = { .buffers = &tx_buf, .count = 1U };

    struct spi_buf rx_buf = { .buf = &rx_byte, .len = 1U };
    struct spi_buf_set rx_set = { .buffers = &rx_buf, .count = 1U };

    while (true)
    {
        int ret = spi_transceive(spi_dev, &spi_cfg, &tx_set, &rx_set);

        if (ret == 0)
        {
            LOG_INF("Received byte: 0x%02x", rx_byte);
        }
        else
        {
            LOG_ERR("SPI transceive failed: %d", ret);
        }

        k_msleep(500);
    }

    return 0;
}

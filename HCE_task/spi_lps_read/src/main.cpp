#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/spi.h>
#include <stdio.h>



#define THS_P_L 0x0C
#define THS_P_H 0x0D
#define MSG 0x0F
#define READ 0x80
#define WRITE 0x00
#define DUMMY_BYTE 0x00


static const struct device *g_spi = DEVICE_DT_GET(DT_NODELABEL(spi1));


int main(void)
{
uint8_t data_tx[2] = {0x8F,0};
uint8_t data_rx[2] = {0}; 

// struct spi_config config = {
// .frequency = 1000000,
// .operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_OP_MODE_MASTER,
// .slave = 0
// };
 
struct spi_config config =
SPI_CONFIG_DT(DT_NODELABEL(lps22hb_press),
SPI_WORD_SET(8) |
SPI_TRANSFER_MSB |
SPI_OP_MODE_MASTER |
SPI_MODE_CPOL |
SPI_MODE_CPHA,
0); 

struct spi_buf tx_buf = {
.buf = &data_tx , .len = 2
};

struct spi_buf rx_buf = {
.buf = &data_rx , .len = 2
};


struct spi_buf_set tx = {
.buffers = &tx_buf,
.count = 1
};

struct spi_buf_set rx = {
.buffers = &rx_buf,
.count = 1
};
spi_transceive(g_spi,&config,&tx,&rx);

printk("WHO_AM_I = 0x%02X\n", data_rx[1]);

while(1)
{
k_msleep(1000);
}

return 0;
}
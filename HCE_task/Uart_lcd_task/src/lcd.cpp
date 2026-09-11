#include "lcd.hpp"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>


namespace {

const struct device* i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));

constexpr uint8_t lcd_addr  = 0x27;
constexpr uint8_t backlight = 0x08;
constexpr uint8_t en_bit    = 0x04;
constexpr uint8_t rs_bit    = 0x01;

void lcd_write4bits(uint8_t data)
{
    uint8_t buf = data | backlight;
    i2c_write(i2c_dev, &buf, 1, lcd_addr);

    buf = data | backlight | en_bit;
    i2c_write(i2c_dev, &buf, 1, lcd_addr);
    k_busy_wait(1);

    buf = data | backlight;
    i2c_write(i2c_dev, &buf, 1, lcd_addr);
    k_busy_wait(50);
}

void lcd_send(uint8_t value, uint8_t mode)
{
    uint8_t high_nibble = (value & 0xF0) | mode;
    uint8_t low_nibble  = static_cast<uint8_t>((value << 4) & 0xF0) | mode;

    lcd_write4bits(high_nibble);
    lcd_write4bits(low_nibble);
}

void lcd_command(uint8_t cmd)
{
    lcd_send(cmd, 0);
}

void lcd_data(uint8_t data)
{
    lcd_send(data, rs_bit);
}

}  // namespace


bool lcd_init()
{
    if (!device_is_ready(i2c_dev))
    {
        return false;
    }

    k_msleep(50);

    lcd_write4bits(0x30);
    k_msleep(5);
    lcd_write4bits(0x30);
    k_msleep(1);
    lcd_write4bits(0x30);
    k_msleep(1);
    lcd_write4bits(0x20);   /* switch to 4-bit mode */

    lcd_command(0x28);      /* 4-bit, 2 line, 5x8 font */
    lcd_command(0x0C);      /* display on, cursor off */
    lcd_command(0x01);      /* clear display */
    k_msleep(2);
    lcd_command(0x06);      /* entry mode: increment cursor */

    return true;
}

void lcd_print(const char* str)
{
    while (*str != '\0')
    {
        lcd_data(static_cast<uint8_t>(*str));
        str++;
    }
}

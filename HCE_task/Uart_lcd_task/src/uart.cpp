#include "uart.hpp"
#include "lcd.hpp"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <cstring>

LOG_MODULE_REGISTER(Uart_task, LOG_LEVEL_INF);


namespace {

const struct device* uart_pin = DEVICE_DT_GET(DT_ALIAS(uart1)) ;
char rx_buf[15];
int i = 0;
}

int uart_run() {

    LOG_INF("Board is booting up");


    if (!device_is_ready(uart_pin)) 
    {
        LOG_ERR("UART device is not ready");
        return -1;
    }

    if (!lcd_init())
    {
        LOG_ERR("LCD device is not ready");
        return -1;
    }
     
      

    LOG_INF("Board is running");

    while (true)
    {
        unsigned char c;

        if (uart_poll_in(uart_pin, &c) == 0)
        {
            uart_poll_out(uart_pin, c);  

            if (c == '\n')
            {
                rx_buf[i] = '\0';
                i = 0;

                if (strcmp(rx_buf, "vanakam") == 0)
                {
                    const char* reply = "vanakam\r\n";

                    while (*reply != '\0')
                    {
                        uart_poll_out(uart_pin, *reply);
                        reply++;
                    }

                    lcd_print("vanakam");
                }
            }
            else if (i < 14)
            {
                rx_buf[i] = c;
                i++;
            }
        }
    }

    return 0;
   
}

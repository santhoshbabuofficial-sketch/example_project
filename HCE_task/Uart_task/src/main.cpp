#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>


LOG_MODULE_REGISTER(Uart_task, LOG_LEVEL_INF);


namespace {

const struct device* uart_dev = DEVICE_DT_GET(DT_ALIAS(uart1)) ;
unsigned char uart_command[15]= "I am alive";
int i=0;
}

int main() {

    LOG_INF("Board is booting up");


    if (!device_is_ready(uart_dev)) 
    {
        LOG_ERR("UART device is not ready");
        return -1;
    }
     
      

    LOG_INF("Board is running");
    
     while (i< 10)
      { 
          uart_poll_out(uart_dev, uart_command[i]);
          i++;
        //    k_msleep(10);
    }

    
     return 0;
   
}
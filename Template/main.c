#include "gd32e23x.h"
#include "systick.h"
#include <stdio.h>
#include "main.h"

#include "../Source/uart/uart.h"
#include "../Source/base/debug.h"
#include "../Source/device/jump_device.h"
#include "../Source/timer/timer.h"
#include "../Source/protocol/protocol.h"
#include "../Source/flash/flash.h"

void app_test(void* arg)
{
    APP_PRINTF("app_test\r\n");
}
int main(void)
{
    systick_config();           // 配置系统定时器

    delay_1ms(1000);                 // 初始化定时器前一定要添加延时
    app_timer_init();                // 初始化定时器

    app_usart_init(0, 115200);  // 初始化业务串口
    app_usart_init(1, 115200);  // 初始化调试串口

    app_proto_init();           // 协议层初始化

    app_jump_device_init();

    uint8_t byte0 = 0x12;  // 第一个字节
    uint8_t byte1 = 0x34;  // 第二个字节
    uint8_t byte2 = 0x56;  // 第三个字节
    uint8_t byte3 = 0x78;  // 第四个字节

    // 将四个字节组合成一个32位的整数
    uint32_t combined = (uint32_t)byte3 << 24 | (uint32_t)byte2 << 16 | (uint32_t)byte1 << 8 | byte0;
    uint32_t read_buffer[4];
    app_flash_init();

    app_flash_write(0, &combined, sizeof(combined));
    app_flash_read(0, read_buffer, sizeof(read_buffer));
    APP_PRINTF_BUF("read_buffer:", read_buffer, sizeof(read_buffer));

    // app_timer_start(1000, app_test, true, NULL);
    while(1) 
    {
        
    }
}
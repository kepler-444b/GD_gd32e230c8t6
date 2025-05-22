#include "gd32e23x.h"
#include "systick.h"
#include <stdio.h>
#include <string.h>
#include "main.h"

#include "../Source/uart/uart.h"
#include "../Source/base/debug.h"
#include "../Source/base/base.h"
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

    delay_1ms(1000);            // 初始化定时器前一定要添加延时
    app_timer_init();           // 初始化定时器

    app_usart_init(0, 115200);  // 初始化业务串口
    app_usart_init(1, 115200);  // 初始化调试串口

    app_proto_init();           // 协议层初始化
    

    app_jump_device_init();
    app_load_config();
    
    APP_PRINTF("my_dev_config.head = %02x\n", my_dev_config.head);
    APP_PRINTF("my_dev_config.channel = %02x\n", my_dev_config.channel);
    APP_PRINTF("my_dev_config.cate = %02x\n", my_dev_config.cate);
    // app_timer_start(1000, app_test, true, NULL);
    
    
    
    while(1) 
    {
        app_timer_poll();
        app_usart_poll();
        APP_DELAY_1MS();
        
    }
}
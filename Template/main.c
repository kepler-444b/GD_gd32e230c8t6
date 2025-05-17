#include "gd32e23x.h"
#include "systick.h"
#include <stdio.h>
#include "main.h"
#include "gd32e230c_eval.h"

#include "../../Source/uart/uart.h"
#include "../../Source/base/debug.h"
#include "../../Source/device/jump_device.h"
#include "../../Source/timer/timer.h"

void app_test(void)
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
    
    app_jump_device_init();

    // app_timer_start(1000, app_test, true);
    while(1) 
    {
        
    }
}
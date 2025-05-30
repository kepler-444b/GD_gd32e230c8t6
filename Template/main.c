#include "gd32e23x.h"
#include "systick.h"
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "../Source/uart/uart.h"
#include "../Source/base/debug.h"
#include "../Source/base/base.h"
#include "../Source/device/jump_device.h"
#include "../Source/protocol/protocol.h"
#include "../Source/flash/flash.h"
#include "../Source/eventbus/eventbus.h"
#include "../Source/pwm/pwm.h"
#include "../Source/watchdog/watchdog.h"

StackType_t AppInitStackBuffer[256]; // 分配256字(word)的栈空间
StaticTask_t AppInitStaticBuffer;

// FreeRTOS 检测堆栈溢出的钩子函数,在这里实现
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask; // 避免未使用变量警告
    APP_ERROR("ERROR: Stack overflow in task %s\n", pcTaskName);
    while (1); // 死循环，便于调试
}

void app_serve_task(void *pvParameters)
{
    // 初始化硬件和外设
    app_usart_init(1, 115200); // 调试串口
    app_usart_init(0, 115200); // 业务串口
    app_eventbus_init();       // 事件总线
    app_proto_init();          // 协议层
    app_watchdog_init();       // 看门狗

    // 加载配置和设备初始化
    app_load_config();
    app_jump_device_init();

    while (1) {
        app_eventbus_poll();
        vTaskDelay(1); // 延时1ms,让出cpu

#if 0 // 打印任务剩余栈空间(word)
        UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
        APP_PRINTF("Stack remaining: %lu words\n", (unsigned long)uxHighWaterMark);
        vTaskDelay(pdMS_TO_TICKS(1000));
#endif
    }
}
int main(void)
{
    systick_config(); // 配置系统定时器

    TaskHandle_t AppInitTaskHandle = xTaskCreateStatic(
        app_serve_task,           // 主任务
        "app_init_task",          // 
        256,                      // 任务堆栈大小(256个word)
        NULL,                     // 任务参数(可传递给任务函数的void指针)
        configMAX_PRIORITIES - 1, // 
        AppInitStackBuffer,      // 静态分配的堆栈空间数组
        &AppInitStaticBuffer);       

    vTaskStartScheduler(); // 启动实时操作系统内核调度器(RTOS内核接管控制权)
}

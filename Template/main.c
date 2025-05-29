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

StackType_t app_init_task_stack[125];
StaticTask_t app_init_task_cb;

// FreeRTOS 检测堆栈溢出的钩子函数,在这里实现
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask; // 避免未使用变量警告
    APP_ERROR("ERROR: Stack overflow in task %s\n", pcTaskName);
    while (1); // 死循环，便于调试
}

void app_serve_task(void *pvParameters)
{
    while (1) {
        app_eventbus_poll();
    }
}
int main(void)
{
    systick_config(); // 配置系统定时器

    app_usart_init(0, 115200); // 初始化业务串口
    app_usart_init(1, 115200); // 初始化调试串口
    app_eventbus_init();       // 初始化事件总线
    app_proto_init();          // 协议层初始化

    app_jump_device_init();
    app_load_config();
    TaskHandle_t vAppIniatTask = xTaskCreateStatic(app_serve_task, "app_init_task", 125, NULL, 1, app_init_task_stack, &app_init_task_cb);
    vTaskStartScheduler(); // 启动实时操作系统内核调度器
}
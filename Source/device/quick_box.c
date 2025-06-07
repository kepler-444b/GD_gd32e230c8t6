#include "gd32e23x.h"
#include "quick_box.h"
#include "device_manager.h"
#include "FreeRTOS.h"
#include "timers.h"
#include "../gpio/gpio.h"
#include "../base/debug.h"
#include "../uart/uart.h"
#include "../protocol/protocol.h"
#include "../pwm/pwm.h"
#include "../eventbus/eventbus.h"

#if defined QUICK_BOX

typedef struct {
    bool key_status;
    uint16_t lum;
    bool mode;
} quick_box_t;
static quick_box_t my_quick_box = {0};

void quick_box_gpio_init(void);
void quick_box_data_cb(valid_data_t *data);
void quick_event_handler(event_type_e event, void *params);
void quick_box_zero(TimerHandle_t xTimer);

void quick_box_init(void)
{
    APP_PRINTF("quick_box_init\n");
    quick_box_gpio_init();

    app_pwm_init(PB7, PB6, PB5, DEFAULT); //  初始化PWM
}

void quick_box_gpio_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);

    gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO_PIN_0); // KEY
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_1);  // D1
    // gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_2); // D2 调试阶段用作 usart1_tx
    // gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_3); // D3 调试阶段用作 usart1_rx
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_4); // CSN
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_5); // SCK
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_6); // MISO
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_7); // MOSI

    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_0); // LD1
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_1); // LD2
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_2); // CE

    // 3路PWM调光
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO_PIN_5); // OUT13
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO_PIN_6); // OUT12
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO_PIN_7); // OUT11

    gpio_mode_set(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_11);  // DET0 过零点检测
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_13); // OUT9
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_14); // OUT8
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_15); // OUT7

    // 上电后,先关闭所有灯
    app_ctrl_gpio(PB5, false);
    app_ctrl_gpio(PB6, false);
    app_ctrl_gpio(PB7, false);

    // 订阅事件总线
    app_eventbus_subscribe(quick_event_handler);

    // 初始化一个静态定时器,用于检测值
    static StaticTimer_t CheckZeroStaticBuffer;
    static TimerHandle_t CheckZeroTimerHandle = NULL;

    CheckZeroTimerHandle = xTimerCreateStatic(
        "ReadAdcTimer",        // 定时器名称(调试用)
        pdMS_TO_TICKS(1),      // 定时周期(1ms)
        pdTRUE,                // 自动重载(TRUE=周期性，FALSE=单次)
        NULL,                  // 定时器ID(可用于传递参数)
        quick_box_zero,        // 回调函数
        &CheckZeroStaticBuffer // 静态内存缓冲区

    );
    // 启动定时器(0表示不阻塞)
    if (xTimerStart(CheckZeroTimerHandle, 0) != pdPASS) {
        APP_ERROR("CheckZeroTimerHandle error");
    }
}

void quick_box_data_cb(valid_data_t *data)
{
    APP_PRINTF_BUF("data", data->data, data->length);
    if (data->data[1] == 0x0d) {
        switch (data->data[7]) // 按键分组
        {
            case 0x00: // 总关
            {
                app_set_pwm_fade(0, 0, 1000);
                app_set_pwm_fade(1, 0, 1000);
                app_set_pwm_fade(2, 0, 1000);
            } break;
            case 0x01: // 明亮
            {
                app_set_pwm_fade(0, 500, 1000);
                app_set_pwm_fade(1, 500, 1000);
                app_set_pwm_fade(2, 500, 1000);
            } break;
            case 0x08: // 温馨
            {
                app_set_pwm_fade(0, 100, 1000);
                app_set_pwm_fade(1, 100, 1000);
                app_set_pwm_fade(2, 100, 1000);
            } break;
        }
    } else if (data->data[1] == 0x00) {
        app_set_pwm_fade(0, 0, 1000);
        app_set_pwm_fade(1, 0, 1000);
        app_set_pwm_fade(2, 0, 1000);
    }
}

void quick_event_handler(event_type_e event, void *params)
{
    switch (event) {
        case EVENT_ENTER_CONFIG: {
            // 进入配置模式
        } break;
        case EVENT_EXIT_CONFIG: {
            // 退出配置模式
        } break;
        case EVENT_RECEIVE_CMD: {
            valid_data_t *valid_data = (valid_data_t *)params;
            quick_box_data_cb(valid_data);
        }
        default:
            return;
    }
}

// 用于检测交流电的零点
void quick_box_zero(TimerHandle_t xTimer)
{
    FlagStatus status = APP_GET_GPIO(PB11);
    // APP_PRINTF("status:%d\n", status);
}
#endif
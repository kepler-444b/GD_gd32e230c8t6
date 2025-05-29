#include "panel_key6.h"
#include "device_manager.h"
#include <float.h>
#include "gd32e23x.h"
#include "FreeRTOS.h"
#include "timers.h"

#include "../base/debug.h"
#include "../base/base.h"
#include "../switch/led.h"
#include "../adc/adc.h"
#include "../gpio/gpio.h"
#include "../uart/uart.h"
#include "../base/panel_base.h"
#include "../eventbus/eventbus.h"
#include "../protocol/protocol.h"
#include "../pwm/pwm.h"

#if defined PANEL_KEY6

#define ADC_TO_VOLTAGE(adc_val) ((adc_val) * 330 / 4096) // adc值转电压
#define KEY_NUMBER_COUNT        6                        // 按键数量
#define ADC_VALUE_COUNT         10                       // 电压值缓冲区数量

static uint16_t adc_value_buffer[ADC_VALUE_COUNT];
static uint8_t adc_index   = 0;
static uint8_t buffer_full = 0; // 标记缓冲区是否已满

// 此结构体用于配置每个按键对应的电压范围
typedef struct
{
    uint16_t min;
    uint16_t max;
} key_config_t;
static const key_config_t my_key_config[KEY_NUMBER_COUNT] = {
    {
        0,
        15,
    }, // 按键1
    {
        25,
        45,
    }, // 按键2
    {
        85,
        110,
    }, // 按键3
    {
        145,
        155,
    }, // 按键4
    {
        195,
        210,
    }, // 按键5
    {
        230,
        255,
    }, // 按键6
};
typedef struct
{
    bool key_status;   // 按键状态
    bool led_w_status; // 白灯状态

} panel_stats;
static panel_stats my_panel_status[KEY_NUMBER_COUNT] = {0};

typedef struct
{
    bool led_flick;           // 闪烁状态
    bool key_long_perss;      // 长按状态
    bool enter_config;        // 进入配置状态
    uint16_t key_long_bumber; // 长按计数
} long_press_t;
static long_press_t my_long_press;

// 函数声明
void panel_gpio_init(void);
void panel_ctrl_status(uint8_t led_num, bool led_state);
void app_panel_read_adc(TimerHandle_t xTimer);
void panel_ctrl_led_all(bool led_state);
void panel_event_handler(event_type_e event, void *params);
void panel_data_cb(valid_data_t *data);

static TaskHandle_t appPanelTaskHandle = NULL;
void panel_key6_init(void)
{
    panel_gpio_init();
    adc_channel_t my_adc_channel;
    my_adc_channel.adc_channel_0 = true; // 只使用0通道

    app_adc_init(&my_adc_channel);
    app_pwm_init(PA8, DEFAULT, DEFAULT, DEFAULT);

    app_set_pwm_fade(0, 500, 3000);
    
    // 订阅事件总线
    app_eventbus_subscribe(panel_event_handler);

    static StaticTimer_t ReadAdcTimerBuffer;
    static TimerHandle_t ReadAdcTimerHandle = NULL;

    ReadAdcTimerHandle = xTimerCreateStatic(
        "ReadAdcTimer",     // 定时器名称(调试用)
        pdMS_TO_TICKS(1),   // 定时周期(1ms)
        pdTRUE,             // 自动重载(TRUE=周期性，FALSE=单次)
        NULL,               // 定时器ID(可用于传递参数)
        app_panel_read_adc, // 回调函数
        &ReadAdcTimerBuffer // 静态内存缓冲区

    );
    // 启动定时器(0表示不阻塞)
    if (xTimerStart(ReadAdcTimerHandle, 0) != pdPASS) {
        APP_ERROR("ReadAdcTimerHandle error");
    }
}

void panel_gpio_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);

    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_8); // 背光灯 GPIOA8

    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_15); // 6个白灯
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_3);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_4);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_5);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_6);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_8);

    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_12); // 4个继电器
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_13);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_14);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_15);
}

void app_panel_read_adc(TimerHandle_t xTimer)
{

    uint16_t adc_value = ADC_TO_VOLTAGE(app_get_adc_value()[0]);
    for (int i = 0; i < KEY_NUMBER_COUNT; i++) {
        if (adc_value >= my_key_config[i].min && adc_value <= my_key_config[i].max) {
            // 填充满buffer才会执行下面的代码，故此行执行10次才执行下面的代码
            adc_value_buffer[adc_index++] = adc_value;

            if (adc_index >= ADC_VALUE_COUNT) {
                adc_index   = 0;
                buffer_full = 1;

                uint16_t new_value = app_calculate_average(adc_value_buffer, ADC_VALUE_COUNT);
                if (new_value >= my_key_config[i].min && new_value <= my_key_config[i].max) {
                    if (my_panel_status[i].key_status == false) {
                        if (my_long_press.enter_config == false) // 如果进入了配置模式，则禁用按键操作
                        {
                            APP_PRINTF("key_down\n");
                            my_panel_status[i].led_w_status = !my_panel_status[i].led_w_status;
                            static bool key_status          = false;
                            key_status                      = !key_status;
                            app_panel_send_cmd(i, my_panel_status[i].led_w_status, 0XF1); // 此处只发送命令，不处理led和继电器

                            my_panel_status[i].key_status = true; // 短按标记为按下
                        }
                        my_long_press.key_long_perss  = true;        // 长按标记为按下
                        my_long_press.key_long_bumber = 0;           // 长按计数器清零
                    } else if (my_long_press.key_long_perss == true) // 进入长按延时阶段
                    {
                        my_long_press.key_long_bumber++;
                        if (my_long_press.key_long_bumber >= 500) // 实际延时 5s
                        {
                            app_panel_send_cmd(0, 0, 0xF8);
                            my_long_press.key_long_perss = false;
                        }
                    }
                }
            }
        } else if (adc_value >= DEFAULT_MIN_VALUE && adc_value <= DEFAULT_MAX_VALUE) // 没有按键按下时
        {
            my_panel_status[i].key_status = false; // 标记为未按下
            my_long_press.key_long_perss  = false; // 重置长按状态
            my_long_press.key_long_bumber = 0;     // 重置长按计数
        }
    }
}
void panel_data_cb(valid_data_t *data)
{
    // APP_PRINTF_BUF("panel_data_cb", data->data, data->length);
    switch (data->data[1]) {
        case 0x0E: { // 灯控模式
            for (uint8_t i = 0; i < 4; i++) {
                if (data->data[3] == my_dev_config.group[i]) {
                    panel_ctrl_status(i, data->data[2]);
                    my_panel_status[i].led_w_status = data->data[2];
                }
            }
            if (data->data[3] == my_dev_config.group_5) {
                panel_ctrl_status(4, data->data[2]);
                my_panel_status[4].led_w_status = data->data[2];
            }
            if (data->data[3] == my_dev_config.group_6) {
                panel_ctrl_status(5, data->data[2]);
                my_panel_status[5].led_w_status = data->data[2];
            }

        } break;
    }
}
void panel_ctrl_status(uint8_t led_num, bool led_state)
{
    gpio_pin_typedef_t led_gpio   = DEFAULT;
    gpio_pin_typedef_t eraly_gpio = DEFAULT;

    // 根据 LED 编号设置 GPIO
    switch (led_num) {
        case 0:
            led_gpio   = PA15;
            eraly_gpio = PB12;
            break;
        case 1:
            led_gpio   = PB3;
            eraly_gpio = PB13;
            break;
        case 2:
            led_gpio   = PB4;
            eraly_gpio = PB14;
            break;
        case 3:
            led_gpio   = PB5;
            eraly_gpio = PB15;
            break;
        case 4:
            led_gpio = PB6;
            break;
        case 5:
            led_gpio = PB8;
            break;
        default:
            return; // 无效 LED 编号直接返回
    }

    // 控制 LED GPIO
    app_ctrl_gpio(led_gpio, led_state);
    if (GPIO_IS_VALID(eraly_gpio)) {
        app_ctrl_gpio(eraly_gpio, led_state);
    }
}

void panel_ctrl_led_all(bool led_state)
{
    const gpio_pin_typedef_t leds[] = {PA15, PB3, PB4, PB5, PB6, PB8};
    const size_t num_leds           = sizeof(leds) / sizeof(leds[0]);

    for (size_t i = 0; i < num_leds; i++) {
        app_ctrl_gpio(leds[i], led_state);
    }
}

void panel_event_handler(event_type_e event, void *params)
{
    APP_PRINTF("event:%d\n", event);
    switch (event) {
        case EVENT_ENTER_CONFIG: {
            panel_ctrl_led_all(true);
            my_long_press.enter_config = true;
        } break;
        case EVENT_EXIT_CONFIG: {
            panel_ctrl_led_all(false);
            my_long_press.enter_config = false;
        } break;
        case EVENT_RECEIVE_CMD: {
            valid_data_t *valid_data = (valid_data_t *)params;
            panel_data_cb(valid_data);
        }
        default:
            return;
    }
}

#endif
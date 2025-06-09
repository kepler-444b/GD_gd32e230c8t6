#include "panel_key.h"
#include "device_manager.h"
#include <float.h>
#include "gd32e23x.h"
#include "FreeRTOS.h"
#include "timers.h"

#include "../base/debug.h"
#include "../base/base.h"
#include "../adc/adc.h"
#include "../gpio/gpio.h"
#include "../uart/uart.h"
#include "../base/panel_base.h"
#include "../eventbus/eventbus.h"
#include "../protocol/protocol.h"
#include "../pwm/pwm.h"
#include "../config/config.h"

#if defined PANEL_KEY

#define ADC_TO_VOLTAGE(adc_val) ((adc_val) * 330 / 4096) // adc值转电压
#define ADC_VALUE_COUNT         10                       // 电压值缓冲区数量

#define DEFAULT_MIN_VALUE       329 // 无按键按下时的最小电压值
#define DEFAULT_MAX_VALUE       330 // 无按键按下时的最大电压值

#define SHORT_TIME_LED          50    // 短亮时间
#define SHORT_TIME_RELAY        30000 // 继电器导通时间
#define SHORT_TIME_BACK         5000  // 一定时间内背光变化

// 此结构体用于处理 adc 数据
typedef struct
{
    uint16_t adc_value_buffer[ADC_VALUE_COUNT];
    uint8_t buffer_index;
    bool buffer_full;
} adc_value_t;
static adc_value_t my_adc_value;

// 此结构体用于配置每个按键对应的电压范围
typedef struct
{
    uint16_t min;
    uint16_t max;
} key_config_t;
static const key_config_t my_key_config[KEY_NUMBER_COUNT] = {

    [0] = {.min = 0, .max = 15},
    [1] = {.min = 25, .max = 45},
    [2] = {.min = 85, .max = 110},
    [3] = {.min = 145, .max = 155},
    [4] = {.min = 195, .max = 210},
    [5] = {.min = 230, .max = 255},
};

typedef struct
{
    bool key_press;  // 按键按下
    bool key_status; // 按键状态

    bool led_open;  // 白灯状态
    bool led_short; // 启用短亮(窗帘开关)
    bool led_last;  // 白灯上次状态

    bool relay_open;  // 继电状态
    bool relay_short; // 继电器短开
    bool relay_last;  // 继电器上次状态

    uint16_t led_short_count;  // 短亮计数器
    uint32_t relay_open_count; // 继电器导通计数器

} panel_status_t;
static panel_status_t my_panel_status[KEY_NUMBER_COUNT] = {0};

typedef struct
{
    bool led_filck;      // 闪烁
    bool key_long_perss; // 长按状态
    bool enter_config;   // 进入配置状态
    uint8_t back_status; // 所有背光灯状态(0:关闭,1:打开,2:低亮)
    uint8_t back_status_last;
    uint16_t key_long_count;  // 长按计数
    uint32_t led_filck_count; // 闪烁计数
} common_panel_t;
static common_panel_t my_common_panel;

// 函数声明
void panel_gpio_init(void);
void panel_read_adc(TimerHandle_t xTimer);
void panel_proce_cmd(TimerHandle_t xTimer);
void panel_ctrl_led_all(bool led_state);
void panel_event_handler(event_type_e event, void *params);
void panel_data_cb(valid_data_t *data);
void panel_check_key_status(void);
void panel_power_status(void);

void panel_key_init(void)
{
    APP_PRINTF("panel_key_init\n");
    panel_gpio_init();
    adc_channel_t my_adc_channel = {0};

#if defined PANEL_6KEY
    my_adc_channel.adc_channel_0 = true; // 只使用0通道
    app_adc_init(&my_adc_channel);
    gpio_pin_typedef_t pins[1] = {PA8};
    app_pwm_init(pins, 1);
    app_set_pwm_fade(0, 500, 3000);

    panel_power_status();
#endif

#if defined PANEL_8KEY
    // my_adc_channel.adc_channel_0 = true;
    // my_adc_channel.adc_channel_1 = true;
    // app_adc_init(&my_adc_channel);

    gpio_pin_typedef_t pins[8] = {PA4, PA5, PA6, PA7, PB6, PB7, PB8, PB9};
    app_pwm_init(pins, 8);

    app_set_pwm_fade(0, 500, 3000);
    app_set_pwm_fade(1, 500, 3000);
    app_set_pwm_fade(2, 500, 3000);
    app_set_pwm_fade(3, 500, 3000);
    app_set_pwm_fade(4, 500, 3000);
    app_set_pwm_fade(5, 500, 3000);
    app_set_pwm_fade(6, 500, 3000);
    app_set_pwm_fade(7, 500, 3000);

    // APP_PRINTF("%d %d\n", app_get_gpio(PB6), app_get_gpio(PB7));

    APP_SET_GPIO(PB0, true);
    APP_SET_GPIO(PB1, true);
    APP_SET_GPIO(PB2, true);
    APP_SET_GPIO(PB3, true);
    APP_SET_GPIO(PB4, true);

    APP_SET_GPIO(PB5, true);
    APP_SET_GPIO(PB10, true);
    APP_SET_GPIO(PA15, true);

    APP_SET_GPIO(PB12, true);
    APP_SET_GPIO(PB13, true);
    APP_SET_GPIO(PB14, true);
    APP_SET_GPIO(PB15, true);
    // APP_PRINTF("%d %d %d %d\n", APP_SET_GPIO(PB12), APP_SET_GPIO(PB13), APP_SET_GPIO(PB14), APP_SET_GPIO(PB15));

#endif
    // 订阅事件总线
    app_eventbus_subscribe(panel_event_handler);
#if 1
    // 初始化一个静态定时器,用于读取adc值
    static StaticTimer_t ReadAdcStaticBuffer;
    static TimerHandle_t ReadAdcTimerHandle = NULL;

    ReadAdcTimerHandle = xTimerCreateStatic(
        "ReadAdcTimer",      // 定时器名称(调试用)
        pdMS_TO_TICKS(1),    // 定时周期(1ms)
        pdTRUE,              // 自动重载(TRUE=周期性，FALSE=单次)
        NULL,                // 定时器ID(可用于传递参数)
        panel_read_adc,      // 回调函数
        &ReadAdcStaticBuffer // 静态内存缓冲区

    );
    // 启动定时器(0表示不阻塞)
    if (xTimerStart(ReadAdcTimerHandle, 0) != pdPASS) {
        APP_ERROR("ReadAdcTimerHandle error");
    }

    static StaticTimer_t PanelStaticBuffer;
    static TimerHandle_t PanleTimerHandle = NULL;

    PanleTimerHandle = xTimerCreateStatic(
        "PanelTimer",      // 定时器名称(调试用)
        pdMS_TO_TICKS(1),  // 定时周期(1ms)
        pdTRUE,            // 自动重载(TRUE=周期性，FALSE=单次)
        NULL,              // 定时器ID(可用于传递参数)
        panel_proce_cmd,   // 回调函数
        &PanelStaticBuffer // 静态内存缓冲区

    );
    // 启动定时器(0表示不阻塞)
    if (xTimerStart(PanleTimerHandle, 0) != pdPASS) {
        APP_ERROR("PanleTimerHandle error");
    }
#endif
}

void panel_gpio_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);

#if defined PANEL_6KEY
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
#endif

#if defined PANEL_8KEY
    // 8 个背光灯
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_4);
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_5);
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_6);
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_7);

    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_6);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_7);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_8);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_9);
    // 8 个白灯
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_0);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_1);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_2);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_10);

    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_15);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_3);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_4);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_5);

    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_12);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_13);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_14);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_15);

#endif
}

void panel_read_adc(TimerHandle_t xTimer)
{
    uint16_t adc_value = ADC_TO_VOLTAGE(app_get_adc_value()[0]);

    for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
        if (adc_value >= my_key_config[i].min && adc_value <= my_key_config[i].max) {
            // 填充满buffer才会执行下面的代码，故此行执行10次才执行下面的代码
            my_adc_value.adc_value_buffer[my_adc_value.buffer_index++] = adc_value;
            if (my_adc_value.buffer_index >= ADC_VALUE_COUNT) {
                my_adc_value.buffer_index = 0;
                my_adc_value.buffer_full  = 1;
                uint16_t new_value        = app_calculate_average(my_adc_value.adc_value_buffer, ADC_VALUE_COUNT);
                if (new_value >= my_key_config[i].min && new_value <= my_key_config[i].max) {
                    if (my_panel_status[i].key_press == false) {
                        if (my_common_panel.enter_config == false) { // 如果进入了配置模式，则禁用按键操作

                            my_panel_status[i].key_status = !my_panel_status[i].key_status;
                            if (my_panel_status[i].relay_short != false) { // 继电器在导通过程中
                                app_send_cmd(i, 0x00, 0xF1, 0x62);
                            } else {
                                app_send_cmd(i, my_panel_status[i].key_status, 0xF1, 0x00);
                            }
                            my_panel_status[i].key_press = true; // 短按标记为按下
                        }
                        my_common_panel.key_long_perss = true; // 长按标记为按下
                        my_common_panel.key_long_count = 0;    // 长按计数器清零
                    } // 进入长按延时阶段
                    else if (my_common_panel.key_long_perss == true) {
                        my_common_panel.key_long_count++;
                        if (my_common_panel.key_long_count >= 500) { // 实际延时 5s
                            app_send_cmd(0, 0, 0xF8, 0x00);
                            my_common_panel.key_long_perss = false;
                        }
                    }
                }
            }
        } // 没有按键按下时
        else if (adc_value >= DEFAULT_MIN_VALUE && adc_value <= DEFAULT_MAX_VALUE) {

            my_panel_status[i].key_press   = false; // 标记为未按下
            my_common_panel.key_long_perss = false; // 重置长按状态
            my_common_panel.key_long_count = 0;     // 重置长按计数
        }
    }

    // 开始闪烁
    if (my_common_panel.led_filck == true) {
        my_common_panel.led_filck_count++;
        if (my_common_panel.led_filck_count <= 500) {
            panel_ctrl_led_all(true);
        } else if (my_common_panel.led_filck_count <= 1000) {
            panel_ctrl_led_all(false);
        } else {
            my_common_panel.led_filck_count = 0;     // 重置计数器
            my_common_panel.led_filck       = false; // 停止闪烁
            panel_ctrl_led_all(true);
        }
    }
}

void panel_proce_cmd(TimerHandle_t xTimer)
{
    const panel_cfg_t *temp_cfg = app_get_dev_cfg();
    for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {

        if (my_panel_status[i].led_short) { // led 短亮
            if (my_panel_status[i].led_short_count == 0) {
                my_panel_status[i].led_open = true;
            }
            my_panel_status[i].led_short_count++;
            if (my_panel_status[i].led_short_count >= SHORT_TIME_LED) {
                my_panel_status[i].led_open        = false;
                my_panel_status[i].led_short       = false;
                my_panel_status[i].led_short_count = 0;
            }
        }

        if (my_panel_status[i].relay_short == true) { // 继电器导通30s
            if (my_panel_status[i].relay_open_count == 0) {
                my_panel_status[i].relay_open = true;
            }
            my_panel_status[i].relay_open_count++;
            if (my_panel_status[i].relay_open_count >= SHORT_TIME_RELAY) {
                my_panel_status[i].relay_open       = false;
                my_panel_status[i].relay_short      = false;
                my_panel_status[i].relay_open_count = 0;
            }
        }

        if (my_panel_status[i].led_open != my_panel_status[i].led_last) { // 开关led
            APP_SET_GPIO(temp_cfg[i].key_led, my_panel_status[i].led_open);
            my_panel_status[i].led_last = my_panel_status[i].led_open;
            panel_check_key_status(); // 每次led变化后都会检测一次
        }
        if (my_panel_status[i].relay_open != my_panel_status[i].relay_last) { // 开关继电器
            // 开关该按键所控的所有继电器
            APP_SET_GPIO(temp_cfg[i].key_relay[0], my_panel_status[i].relay_open);
            APP_SET_GPIO(temp_cfg[i].key_relay[1], my_panel_status[i].relay_open);
            APP_SET_GPIO(temp_cfg[i].key_relay[2], my_panel_status[i].relay_open);
            APP_SET_GPIO(temp_cfg[i].key_relay[3], my_panel_status[i].relay_open);
            // APP_PRINTF("%d%d%d%d\n", APP_GET_GPIO(PB12), APP_GET_GPIO(PB13), APP_GET_GPIO(PB14), APP_GET_GPIO(PB15));
            my_panel_status[i].relay_last = my_panel_status[i].relay_open;

            my_panel_status[i].relay_open_count = 0;
        }
#if defined PANEL_6KEY // 6键面板与8键的背光灯开关逻辑不通,在此用宏定义隔开
        if (my_common_panel.back_status != my_common_panel.back_status_last) {
            if (my_common_panel.back_status == 0) {
                app_set_pwm_fade(0, 0, 5000);
            } else if (my_common_panel.back_status == 1) {
                app_set_pwm_fade(0, 500, 1000);
            } else if (my_common_panel.back_status == 2) {
                app_set_pwm_fade(0, 50, 5000);
            }
            my_common_panel.back_status_last = my_common_panel.back_status;
        }
#endif
    }
}

void panel_check_key_status(void)
{
#if defined PANEL_6KEY
    uint8_t all_key_status = 0;
    for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
        all_key_status += my_panel_status[i].key_status;
    }
    if (all_key_status == 0) {
        my_common_panel.back_status = 2;
    } else {
        my_common_panel.back_status = 1;
    }
#endif
}

void panel_data_cb(valid_data_t *data)
{
    const panel_cfg_t *temp_cfg = app_get_dev_cfg();
    switch (data->data[1]) {

        case 0x00: { // 总关
            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
                if ((temp_cfg[i].key_area >> 4) == (data->data[4] >> 4) ||
                    (data->data[4] >> 4) == 0xF) {         // 匹配"总关区域"(若为0xF则可以控制所有总关区域)
                    if (temp_cfg[i].key_func == 0x0F) {    // 匹配"夜灯"
                        if (temp_cfg[i].key_perm & 0x20) { // "夜灯"受"总关"控制
                            my_panel_status[i].key_status = data->data[2];
                            my_panel_status[i].led_open   = data->data[2];
                            my_panel_status[i].relay_open = data->data[2];
                        }
                    } else if (temp_cfg[i].key_func == 0x0D ||
                               temp_cfg[i].key_func == 0x0E ||
                               temp_cfg[i].key_func == 0x01) { // 匹配"场景""灯控""总关"

                        if (temp_cfg[i].key_perm & 0x20) { // 受"总关"控制
                            my_panel_status[i].key_status = false;
                            my_panel_status[i].led_open   = false;
                            my_panel_status[i].relay_open = false;
                        }
                    }
                    if (temp_cfg[i].key_func == data->data[1] &&
                        temp_cfg[i].key_group == data->data[3] &&
                        temp_cfg[i].key_group != 0x00) { // 匹配双控

                        my_panel_status[i].led_short  = true;
                        my_panel_status[i].relay_open = data->data[2];
                        my_panel_status[i].key_status = data->data[2];
                    }
                }
            }
        } break;
        case 0x01: { // 总开关
            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
                if ((temp_cfg[i].key_area >> 4) == (data->data[4] >> 4) ||
                    (data->data[4] >> 4 == 0xF)) { // 匹配"总关区域"(若为0xF则可以控制所有总关区域)
                    if (temp_cfg[i].key_func == 0x0F) {
                        if (temp_cfg[i].key_perm & 0x01) {
                            my_panel_status[i].key_status = !data->data[2];
                            my_panel_status[i].led_open   = !data->data[2];
                            my_panel_status[i].relay_open = !data->data[2];
                        }
                    } else if (temp_cfg[i].key_func == 0x0D ||
                               temp_cfg[i].key_func == 0x0E ||
                               temp_cfg[i].key_func == 0x01) {

                        if (temp_cfg[i].key_perm & 0x01) {
                            my_panel_status[i].key_status = data->data[2];
                            my_panel_status[i].led_open   = data->data[2];
                            my_panel_status[i].relay_open = data->data[2];
                        }
                    }
                    if (temp_cfg[i].key_func == 0x00) {
                        // 匹配到总关(短亮)
                        my_panel_status[i].led_short  = true;
                        my_panel_status[i].relay_open = data->data[2];
                        my_panel_status[i].key_status = data->data[2];
                    }
                    if (temp_cfg[i].key_func == data->data[1] &&
                        temp_cfg[i].key_group == data->data[3] &&
                        temp_cfg[i].key_group != 0x00) {
                        // 匹配双控
                        my_panel_status[i].led_open   = data->data[2];
                        my_panel_status[i].relay_open = data->data[2];
                        my_panel_status[i].key_status = data->data[2];
                    }
                }
            }

        } break;
        case 0x02: { // 清理房间
            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
                if (temp_cfg[i].key_func == data->data[1] &&
                    temp_cfg[i].key_group == data->data[3] &&
                    temp_cfg[i].key_group != 0x00) {
                    my_panel_status[i].led_open   = data->data[2];
                    my_panel_status[i].relay_open = data->data[2];
                    my_panel_status[i].key_status = data->data[2];
                    if (data->data[2] == true) { // 开启"清理房间"关闭"请勿打扰"
                        for (uint8_t j = 0; j < KEY_NUMBER_COUNT; j++) {
                            if (temp_cfg[j].key_func == 0x03) {
                                my_panel_status[j].led_open   = false;
                                my_panel_status[j].relay_open = false;
                                my_panel_status[j].key_status = false;
                            }
                        }
                    }
                }
            }

        } break;
        case 0x03: { // 请勿打扰
            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
                if (temp_cfg[i].key_func == data->data[1] &&
                    temp_cfg[i].key_group == data->data[3] &&
                    temp_cfg[i].key_group != 0x00) {
                    my_panel_status[i].led_open   = data->data[2];
                    my_panel_status[i].relay_open = data->data[2];
                    my_panel_status[i].key_status = data->data[2];
                    if (data->data[2] == true) { // 开启"请勿打扰"关闭"清理房间"
                        for (uint8_t j = 0; j < KEY_NUMBER_COUNT; j++) {
                            if (temp_cfg[j].key_func == 0x02) {
                                my_panel_status[j].led_open   = false;
                                my_panel_status[j].relay_open = false;
                                my_panel_status[j].key_status = false;
                            }
                        }
                    }
                }
            }

        } break;
        case 0x04: { // 请稍候
            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
                if (temp_cfg[i].key_func == data->data[1] &&
                    temp_cfg[i].key_group == data->data[3] &&
                    temp_cfg[i].key_group != 0x00) {
                    my_panel_status[i].led_open   = data->data[2];
                    my_panel_status[i].relay_open = data->data[2];
                    my_panel_status[i].key_status = data->data[2];
                }
            }

        } break;
        case 0x05: { // 退房
            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
                if (temp_cfg[i].key_func == data->data[1] &&
                    temp_cfg[i].key_group == data->data[3] &&
                    temp_cfg[i].key_group != 0x00) {
                    my_panel_status[i].led_open   = data->data[2];
                    my_panel_status[i].relay_open = data->data[2];
                    my_panel_status[i].key_status = data->data[2];
                }
            }

        } break;
        case 0x06: { // 紧急呼叫
            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
                if (temp_cfg[i].key_func == data->data[1] &&
                    temp_cfg[i].key_group == data->data[3] &&
                    temp_cfg[i].key_group != 0x00) {
                    my_panel_status[i].led_open   = data->data[2];
                    my_panel_status[i].relay_open = data->data[2];
                    my_panel_status[i].key_status = data->data[2];
                }
            }

        } break;
        case 0x07: { // 请求服务
            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
                if (temp_cfg[i].key_func == data->data[1] &&
                    temp_cfg[i].key_group == data->data[3] &&
                    temp_cfg[i].key_group != 0x00) {
                    my_panel_status[i].led_open   = data->data[2];
                    my_panel_status[i].relay_open = data->data[2];
                    my_panel_status[i].key_status = data->data[2];
                }
            }

        } break;
        case 0x10: { // 窗帘开
            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
                // 检查功能匹配和分组匹配
                if (temp_cfg[i].key_func == data->data[1] &&
                    temp_cfg[i].key_group == data->data[3] &&
                    temp_cfg[i].key_group != 0x00) {
                    for (uint8_t j = 0; j < KEY_NUMBER_COUNT; j++) {
                        // 关闭同分组的"窗帘关"
                        if (temp_cfg[j].key_func == 0x11) {
                            my_panel_status[j].relay_short = false;
                        }
                    }
                    my_panel_status[i].led_short   = true; // 短亮
                    my_panel_status[i].relay_short = true; // 继电器导通 30s
                }
            }
        } break;
        case 0x11: { // 窗帘关
            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
                // 检查功能匹配和分组匹配
                if (temp_cfg[i].key_func == data->data[1] &&
                    temp_cfg[i].key_group == data->data[3] &&
                    temp_cfg[i].key_group != 0x00) {
                    for (uint8_t j = 0; j < KEY_NUMBER_COUNT; j++) {
                        // 关闭同分组的"窗帘开"
                        if (temp_cfg[j].key_func == 0x10) {
                            my_panel_status[j].relay_short = false;
                        }
                    }
                    my_panel_status[i].led_short   = true;
                    my_panel_status[i].relay_short = true;
                }
            }
        } break;

        case 0x0D: { // 场景模式
            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {

                if ((((temp_cfg[i].key_area & 0x0F) == (data->data[4] & 0x0F)) ||
                     ((data->data[4] & 0x0F) == 0xF)) &&
                    (temp_cfg[i].key_func == data->data[1])) {          // 匹配"场景区域"(若为0xF则可以控制所有场景区域)和"按键功能"
                    if (temp_cfg[i].key_scene_group == data->data[7]) { // 匹配"场景分组"
                        for (uint8_t j = 0; j < KEY_NUMBER_COUNT; j++) {
                            if (temp_cfg[j].key_func == 0x0D &&
                                (((temp_cfg[j].key_area & 0x0F) == (data->data[4] & 0x0F)) ||
                                 ((data->data[4] & 0x0F) == 0xF))) { // 关闭同"场景区域"的其他分组
                                my_panel_status[j].led_open   = false;
                                my_panel_status[j].relay_open = false;
                                my_panel_status[j].key_status = false;
                            }
                        }
                        my_panel_status[i].led_open   = data->data[2];
                        my_panel_status[i].relay_open = data->data[2];
                        my_panel_status[i].key_status = data->data[2];
                    }
                }
            }

        } break;
        case 0x0E: { // 灯控模式
            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
                if (temp_cfg[i].key_func == data->data[1] &&
                    temp_cfg[i].key_group == data->data[3] &&
                    temp_cfg[i].key_group != 0x00) {

                    my_panel_status[i].led_open   = data->data[2];
                    my_panel_status[i].relay_open = data->data[2];
                    my_panel_status[i].key_status = data->data[2];
                }
            }

        } break;
        case 0x0F: { // 夜灯
            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
                if (temp_cfg[i].key_func == data->data[1] &&
                    temp_cfg[i].key_group == data->data[3] &&
                    temp_cfg[i].key_group != 0x00) {
                    my_panel_status[i].led_open   = data->data[2];
                    my_panel_status[i].relay_open = data->data[2];
                    my_panel_status[i].key_status = data->data[2];
                }
            }

        } break;
        case 0x18: { // 调光上键

        } break;
        case 0x1B: { // 调光下键

        } break;
        case 0x1E: { // 调光3

        } break;
        case 0x21: { // 调光4

        } break;
        case 0x60: { // 开锁

        } break;
        case 0x61: { // tf/蓝牙

        } break;
        case 0x62: { // 窗帘停
            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
                if (temp_cfg[i].key_group == data->data[3] &&
                    temp_cfg[i].key_group != 0x00 &&
                    my_panel_status[i].relay_short == true) { // 匹配分组后如果该继电器正在动作才能触发暂停

                    my_panel_status[i].led_short   = true;
                    my_panel_status[i].relay_short = false;
                    my_panel_status[i].relay_open  = false;
                }
            }
        } break;
        case 0x63: { // 音量+
        } break;
        case 0x64: { // 音量-
        } break;
        case 0x65: { // 播放/暂停
        } break;
        case 0x66: { // 下一首
        } break;
        case 0x67: { // 上一首
        } break;
        default:
            return;
    }
}

void panel_ctrl_led_all(bool led_state)
{
    for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
        my_panel_status[i].led_open = led_state;
    }
}

void panel_event_handler(event_type_e event, void *params)
{
    switch (event) {
        case EVENT_ENTER_CONFIG: {
            panel_ctrl_led_all(true);
            my_common_panel.enter_config = true;
        } break;
        case EVENT_EXIT_CONFIG: {
            panel_ctrl_led_all(false);
            my_common_panel.enter_config = false;
        } break;
        case EVENT_SAVE_SUCCESS: {
            my_common_panel.led_filck = true;
            app_load_config();
        } break;
        case EVENT_RECEIVE_CMD: {
            valid_data_t *valid_data = (valid_data_t *)params;
            panel_data_cb(valid_data);
        } break;
        default:
            return;
    }
}
void panel_power_status(void)
{
    const panel_cfg_t *temp_cfg = app_get_dev_cfg();
    for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
        APP_PRINTF("%02X\n", temp_cfg[i].key_perm);
        if (temp_cfg[i].key_perm & 0x08) { // 权限是否勾选"迎宾"
            my_panel_status[i].led_open   = true;
            my_panel_status[i].relay_open = true;
            my_panel_status[i].key_status = true;
        }
    }
}
#endif
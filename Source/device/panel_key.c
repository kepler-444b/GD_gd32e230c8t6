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

#if defined PANEL_KEY

#define PANEL_6KEY
// #define PANEL_8KEY

#if defined PANEL_6KEY
#define KEY_NUMBER_COUNT 6 // 按键数量
#endif

#if defined PANEL_8KEY
#define KEY_NUMBER_COUNT 8 // 按键数量
#endif

#define ADC_TO_VOLTAGE(adc_val) ((adc_val) * 330 / 4096) // adc值转电压
#define ADC_VALUE_COUNT         10                       // 电压值缓冲区数量

#define DEFAULT_MIN_VALUE       329 // 无按键按下时的最小电压值
#define DEFAULT_MAX_VALUE       330 // 无按键按下时的最大电压值

#define SHORT_TIME_LED          50    // 短亮时间
#define SHORT_TIME_RELAY        30000 // 继电器导通时间

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
    bool led_last;

    bool relay_open;  // 继电状态
    bool relay_short; // 继电器短开
    bool relay_last;

    uint16_t led_filck_count;  // 短亮计数器
    uint32_t relay_open_count; // 继电器导通计数器

    uint8_t key_func;             // 按键功能
    uint8_t key_group;            // 按键分组
    uint8_t key_area;             // 按键区域
    uint8_t key_perm;             // 按键权限
    uint8_t key_scene_group;      // 场景分组
    gpio_pin_typedef_t key_relay; // 按键所控继电器
    gpio_pin_typedef_t key_led;   // 按键所控led

} panel_status_t;
static panel_status_t my_panel_status[KEY_NUMBER_COUNT] = {0};

typedef struct
{
    bool led_filck;           // 闪烁
    bool key_long_perss;      // 长按状态
    bool enter_config;        // 进入配置状态
    uint16_t key_long_count;  // 长按计数
    uint32_t led_filck_count; // 闪烁计数
} long_press_t;
static long_press_t my_long_press;

// 函数声明
void panel_gpio_init(void);
void panel_read_adc(TimerHandle_t xTimer);
void panel_proce_cmd(TimerHandle_t xTimer);
void panel_ctrl_led_all(bool led_state);
void panel_event_handler(event_type_e event, void *params);
void panel_data_cb(valid_data_t *data);
gpio_pin_typedef_t panel_get_relay_num(uint8_t key_num);
void panel_get_key_info(void);

void panel_key_init(void)
{
    APP_PRINTF("panel_key_init\n");
    panel_gpio_init();
    adc_channel_t my_adc_channel;

#if defined PANEL_6KEY
    my_adc_channel.adc_channel_0 = true; // 只使用0通道
    app_adc_init(&my_adc_channel);
    app_pwm_init(PA8, DEFAULT, DEFAULT, DEFAULT);
    app_set_pwm_fade(0, 500, 3000);
#endif

#if defined PANEL_8KEY
    my_adc_channel.adc_channel_0 = true; // 只使用0通道
    my_adc_channel.adc_channel_1 = true; // 只使用0通道
    app_adc_init(&my_adc_channel);
    app_pwm_init(PB6, PB7, DEFAULT, DEFAULT);
    app_set_pwm_fade(0, 500, 3000);
    app_set_pwm_fade(1, 500, 3000);
    app_ctrl_gpio(PB7, true);
    app_panel_send_cmd(0, 0x00, 0xF1, 0x62);

#endif

    panel_get_key_info();
    // 订阅事件总线
    app_eventbus_subscribe(panel_event_handler);

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
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_6); // 背光灯 PB6
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_7); // 背光灯 PB7

    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_0); // 8个白灯
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_1);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_2);
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_8);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_11);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_10);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_9);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_8);
#endif
}

void panel_get_key_info(void)
{
    for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
        my_panel_status[i].key_func        = (i < 4) ? my_dev_config.func[i] : (i == 4) ? my_dev_config.func_5
                                                                                        : my_dev_config.func_6;
        my_panel_status[i].key_group       = (i < 4) ? my_dev_config.group[i] : (i == 4) ? my_dev_config.group_5
                                                                                         : my_dev_config.group_6;
        my_panel_status[i].key_area        = (i < 4) ? my_dev_config.area[i] : (i == 4) ? my_dev_config.area_5
                                                                                        : my_dev_config.area_6;
        my_panel_status[i].key_perm        = (i < 4) ? my_dev_config.perm[i] : (i == 4) ? my_dev_config.perm_5
                                                                                        : my_dev_config.perm_6;
        my_panel_status[i].key_scene_group = (i < 4) ? my_dev_config.perm[i] : (i == 4) ? my_dev_config.scene_group_5
                                                                                        : my_dev_config.scene_group_6;
        my_panel_status[i].key_relay       = panel_get_relay_num(i);
    }
    my_panel_status[0].key_led = PA15;
    my_panel_status[1].key_led = PB3;
    my_panel_status[2].key_led = PB4;
    my_panel_status[3].key_led = PB5;
    my_panel_status[4].key_led = PB6;
    my_panel_status[5].key_led = PB8;

    for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
        APP_PRINTF("[%02X] [%02X] [%02X] [%02X] [%s]",
                   my_panel_status[i].key_func,
                   my_panel_status[i].key_group,
                   my_panel_status[i].key_area,
                   my_panel_status[i].key_perm,
                   app_get_gpio_name(my_panel_status[i].key_relay));
        APP_PRINTF("[%s] ", app_get_gpio_name(my_panel_status[i].key_led));
        APP_PRINTF("\n");
    }
    APP_PRINTF("\n");
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
                        if (my_long_press.enter_config == false) { // 如果进入了配置模式，则禁用按键操作

                            my_panel_status[i].key_status = !my_panel_status[i].key_status;
                            if (my_panel_status[i].relay_short != false) { // 继电器在导通过程中
                                app_panel_send_cmd(i, 0x00, 0xF1, 0x62);
                            } else {
                                app_panel_send_cmd(i, my_panel_status[i].key_status, 0xF1, 0x00);
                            }

                            my_panel_status[i].key_press = true; // 短按标记为按下
                        }
                        my_long_press.key_long_perss = true; // 长按标记为按下
                        my_long_press.key_long_count = 0;    // 长按计数器清零
                    } else if (my_long_press.key_long_perss == true) {
                        // 进入长按延时阶段
                        my_long_press.key_long_count++;
                        if (my_long_press.key_long_count >= 500) {
                            // 实际延时 5s
                            app_panel_send_cmd(0, 0, 0xF8, 0x00);
                            my_long_press.key_long_perss = false;
                        }
                    }
                }
            }
        } else if (adc_value >= DEFAULT_MIN_VALUE && adc_value <= DEFAULT_MAX_VALUE) {
            // 没有按键按下时
            my_panel_status[i].key_press = false; // 标记为未按下
            my_long_press.key_long_perss = false; // 重置长按状态
            my_long_press.key_long_count = 0;     // 重置长按计数
        }
    }
    if (my_long_press.led_filck == true) {
        my_long_press.led_filck_count++;
        if (my_long_press.led_filck_count <= 500) {
            panel_ctrl_led_all(true);
        } else if (my_long_press.led_filck_count <= 1000) {
            panel_ctrl_led_all(false);
        } else {
            my_long_press.led_filck_count = 0;     // 重置计数器
            my_long_press.led_filck       = false; // 停止闪烁
            panel_ctrl_led_all(true);
        }
    }
}

void panel_proce_cmd(TimerHandle_t xTimer)
{
    for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {

        if (my_panel_status[i].led_short) {
            if (my_panel_status[i].led_filck_count == 0) {
                my_panel_status[i].led_open = true;
            }
            my_panel_status[i].led_filck_count++;
            if (my_panel_status[i].led_filck_count >= SHORT_TIME_LED) {
                my_panel_status[i].led_open        = false;
                my_panel_status[i].led_short       = false;
                my_panel_status[i].led_filck_count = 0;
            }
        }

        if (my_panel_status[i].relay_short == true) {
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

        if (my_panel_status[i].led_open != my_panel_status[i].led_last) {
            app_ctrl_gpio(my_panel_status[i].key_led, my_panel_status[i].led_open);
            my_panel_status[i].led_last = my_panel_status[i].led_open;
        }
        if (my_panel_status[i].relay_open != my_panel_status[i].relay_last) {
            app_ctrl_gpio(my_panel_status[i].key_relay, my_panel_status[i].relay_open);
            my_panel_status[i].relay_last = my_panel_status[i].relay_open;

            my_panel_status[i].relay_open_count = 0;
        }
    }
}

void panel_data_cb(valid_data_t *data)
{
    APP_PRINTF("1\n");
    switch (data->data[1]) {

        case 0x00: { // 总关
            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
                if (((my_panel_status[i].key_area >> 4) & 0x0F) == ((data->data[4] >> 4) & 0x0F)) { // 匹配有"总开关"权限的按键
                    if (my_panel_status[i].key_func == 0x0F) {
                        // 匹配到夜灯,打开
                        my_panel_status[i].key_status = data->data[2];
                        my_panel_status[i].led_open   = data->data[2];
                        my_panel_status[i].relay_open = data->data[2];
                    } else {
                        // 匹配到其他,关闭
                        my_panel_status[i].key_status = false;
                        my_panel_status[i].led_open   = false;
                        my_panel_status[i].relay_open = false;
                    }
                }
                if (data->data[1] == my_panel_status[i].key_func) {
                    // 匹配双控
                    my_panel_status[i].led_short  = true;
                    my_panel_status[i].relay_open = data->data[2];
                    my_panel_status[i].key_status = data->data[2];
                }
            }
        } break;
        case 0x01: { // 总开关
            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
                if (((my_panel_status[i].key_area >> 4) & 0x0F) == ((data->data[4] >> 4) & 0x0F)) { // 匹配有"总开关"权限的按键
                    if (my_panel_status[i].key_func == 0x0F) {
                        // 匹配到夜灯,打开
                        my_panel_status[i].key_status = !data->data[2];
                        my_panel_status[i].led_open   = !data->data[2];
                        my_panel_status[i].relay_open = !data->data[2];
                    } else {
                        // 匹配到其他,关闭
                        my_panel_status[i].key_status = data->data[2];
                        my_panel_status[i].led_open   = data->data[2];
                        my_panel_status[i].relay_open = data->data[2];
                    }
                    if (my_panel_status[i].key_func == 0x00) {
                        // 匹配到总关,短亮
                        my_panel_status[i].led_short  = true;
                        my_panel_status[i].relay_open = data->data[2];
                        my_panel_status[i].key_status = data->data[2];
                    }
                    if (data->data[1] == my_panel_status[i].key_func) {
                        // 匹配双控
                        my_panel_status[i].led_open   = data->data[2];
                        my_panel_status[i].relay_open = data->data[2];
                        my_panel_status[i].key_status = data->data[2];
                    }
                }
            }

        } break;
        case 0x02: { // 清理房间

        } break;
        case 0x03: { // 请勿打扰

        } break;
        case 0x04: { // 请稍候

        } break;
        case 0x05: { // 退房

        } break;
        case 0x06: { // 紧急呼叫

        } break;
        case 0x07: { // 请求服务

        } break;
        case 0x10: { // 窗帘开
            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
                // 检查功能匹配和分组匹配
                if (data->data[1] == my_panel_status[i].key_func &&
                    data->data[3] == my_panel_status[i].key_group) {
                    for (uint8_t j = 0; j < KEY_NUMBER_COUNT; j++) {
                        // 关闭同分组的"窗帘关"
                        if (my_panel_status[j].key_func == 0x11) {
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
                if (data->data[1] == my_panel_status[i].key_func &&
                    data->data[3] == my_panel_status[i].key_group) {
                    for (uint8_t j = 0; j < KEY_NUMBER_COUNT; j++) {
                        // 关闭同分组的"窗帘开"
                        if (my_panel_status[j].key_func == 0x10) {
                            my_panel_status[j].relay_short = false;
                        }
                    }
                    my_panel_status[i].led_short   = true;
                    my_panel_status[i].relay_short = true;
                }
            }
        } break;

        case 0x0D: { // 场景模式

        } break;
        case 0x0E: { // 灯控模式
            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
                if (data->data[1] == my_panel_status[i].key_func &&
                    data->data[3] == my_panel_status[i].key_group) {

                    my_panel_status[i].led_open   = data->data[2];
                    my_panel_status[i].relay_open = data->data[2];
                }
            }

        } break;
        case 0x0F: { // 夜灯
            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
                if (data->data[1] == my_panel_status[i].key_func &&
                    data->data[3] == my_panel_status[i].key_group) {
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
                if (data->data[3] == my_panel_status[i].key_group &&
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
            my_long_press.enter_config = true;
        } break;
        case EVENT_EXIT_CONFIG: {
            panel_ctrl_led_all(false);
            my_long_press.enter_config = false;
        } break;
        case EVENT_SAVE_SUCCESS: {
            my_long_press.led_filck = true;
            panel_get_key_info();
        } break;
        case EVENT_RECEIVE_CMD: {
            valid_data_t *valid_data = (valid_data_t *)params;
            panel_data_cb(valid_data);
        } break;

        default:
            return;
    }
}

// 获取按键号操作对应的继电器
gpio_pin_typedef_t panel_get_relay_num(uint8_t key_num)
{
    uint8_t value;
    switch (key_num) {
        case 0:
            value = my_dev_config.reco_h & 0x0F;
            break;
        case 1:
            value = my_dev_config.reco_h >> 4;
            break;
        case 2:
            value = my_dev_config.reco_l & 0x0F;
            break;
        case 3:
            value = my_dev_config.reco_l >> 4;
            break;
        case 4:
            value = my_dev_config.channel & 0x0F;
            break;
        case 5:
            value = my_dev_config.channel >> 4;
            break;
        default:
            return DEFAULT;
    }

    switch (value) {
        case 1:
            return PB12;
        case 2:
            return PB13;
        case 4:
            return PB14;
        case 8:
            return PB15;
        default:
            return DEFAULT;
    }
}
#endif
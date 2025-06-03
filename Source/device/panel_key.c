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

#define ADC_TO_VOLTAGE(adc_val) ((adc_val) * 330 / 4096) // adc值转电压
#define KEY_NUMBER_COUNT        6                        // 按键数量
#define ADC_VALUE_COUNT         10                       // 电压值缓冲区数量

#define DEFAULT_MIN_VALUE       329 // 无按键按下时的最小电压值
#define DEFAULT_MAX_VALUE       330 // 无按键按下时的最大电压值

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
    bool key_status;      // 按键状态
    bool led_w_status;    // 白灯状态
    bool relay_status;    // 继电状态
    bool enable_short;    // 启用短亮
    uint16_t count;       // 短亮计数器
    uint32_t relay_count; // 继电器导通计数器

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
void panel_ctrl_led(uint8_t key_num, bool led_status);
void panel_ctrl_relay(uint8_t key_num, bool relay_status);
void panel_read_adc(TimerHandle_t xTimer);
void panel_ctrl_led_all(bool led_state);
void panel_event_handler(event_type_e event, void *params);
void panel_data_cb(valid_data_t *data);
gpio_pin_typedef_t panel_get_relay_num(uint8_t key_num);

void panel_key_init(void)
{
    APP_PRINTF("panel_key_init\n");
    panel_gpio_init();
    adc_channel_t my_adc_channel;
    my_adc_channel.adc_channel_0 = true; // 只使用0通道

    app_adc_init(&my_adc_channel);
    app_pwm_init(PA8, DEFAULT, DEFAULT, DEFAULT);

    app_set_pwm_fade(0, 500, 3000);

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

void panel_read_adc(TimerHandle_t xTimer)
{
    uint16_t adc_value = ADC_TO_VOLTAGE(app_get_adc_value()[0]);
    // APP_PRINTF("adc_value:%d\n", adc_value);
    for (int i = 0; i < KEY_NUMBER_COUNT; i++) {
        if (adc_value >= my_key_config[i].min && adc_value <= my_key_config[i].max) {
            // 填充满buffer才会执行下面的代码，故此行执行10次才执行下面的代码
            my_adc_value.adc_value_buffer[my_adc_value.buffer_index++] = adc_value;

            if (my_adc_value.buffer_index >= ADC_VALUE_COUNT) {
                my_adc_value.buffer_index = 0;
                my_adc_value.buffer_full  = 1;
                uint16_t new_value        = app_calculate_average(my_adc_value.adc_value_buffer, ADC_VALUE_COUNT);
                if (new_value >= my_key_config[i].min && new_value <= my_key_config[i].max) {
                    if (my_panel_status[i].key_status == false) {
                        if (my_long_press.enter_config == false) // 如果进入了配置模式，则禁用按键操作
                        {
                            APP_PRINTF("key_down[%d]\n", i);
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
        if (my_panel_status[i].enable_short == true) { // 用于窗帘控制

            if (my_panel_status->count == 0) {
                panel_ctrl_led(i, true);
                my_panel_status[i].relay_status = true; // 该按键对应的继电器标记为"导通"
            }
            my_panel_status->count++;
            if (my_panel_status->count >= 50) {
                my_panel_status[i].enable_short = false;
                panel_ctrl_led(i, false);
                my_panel_status->count = 0;
            }
        }
        if (my_panel_status[i].relay_status == true) {
            if (my_panel_status[i].relay_count == 0) {
                panel_ctrl_relay(i, true);
            }
            my_panel_status[i].relay_count++;
            if (my_panel_status[i].relay_count >= 30000) {
                panel_ctrl_relay(i, false);
                my_panel_status[i].relay_count  = 0;
                my_panel_status[i].relay_status = false;
            }
        }
    }
}
void panel_data_cb(valid_data_t *data)
{
    switch (data->data[1]) {

        case 0x00: { // 总关

        } break;
        case 0x01: { // 总开关

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
            for (uint8_t i = 0; i < 4; i++) {
                if (data->data[1] == my_dev_config.func[i]) {      // 匹配按键功能
                    if (data->data[3] == my_dev_config.group[i]) { // 匹配场景分组
                        my_panel_status[i].enable_short = true;
                    }
                }
            }
            if (data->data[1] == my_dev_config.func_5) {
                if (data->data[3] == my_dev_config.group_5) {
                    my_panel_status[4].enable_short = true;
                }
            }
            if (data->data[3] == my_dev_config.group_6) {
                if (data->data[3] == my_dev_config.group_6) {
                    my_panel_status[5].enable_short = true;
                }
            }
        } break;
        case 0x11: { // 窗帘关

        } break;

        case 0x0D: { // 场景模式

        } break;
        case 0x0E: { // 灯控模式
            for (uint8_t i = 0; i < 4; i++) {
                if (data->data[3] == my_dev_config.group[i]) {
                    panel_ctrl_led(i, data->data[2]);
                    my_panel_status[i].led_w_status = data->data[2];
                }
            }
            if (data->data[3] == my_dev_config.group_5) {
                panel_ctrl_led(4, data->data[2]);
                my_panel_status[4].led_w_status = data->data[2];
            }
            if (data->data[3] == my_dev_config.group_6) {
                panel_ctrl_led(5, data->data[2]);
                my_panel_status[5].led_w_status = data->data[2];
            }

        } break;
        case 0x0F: { // 夜灯

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
void panel_ctrl_led(uint8_t key_num, bool led_status)
{
    gpio_pin_typedef_t led_gpio = DEFAULT;

    // 根据 key 编号设置 GPIO
    switch (key_num) {
        case 0:
            led_gpio = PA15;
            break;
        case 1:
            led_gpio = PB3;
            break;
        case 2:
            led_gpio = PB4;
            break;
        case 3:
            led_gpio = PB5;
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
    if (GPIO_IS_VALID(led_gpio)) {
        app_ctrl_gpio(led_gpio, led_status);
    }
}
void panel_ctrl_relay(uint8_t key_num, bool relay_status)
{
    gpio_pin_typedef_t eraly_gpio = DEFAULT;
    switch (key_num) {
        case 0:
            eraly_gpio = panel_get_relay_num(0);
            break;
        case 1:
            eraly_gpio = panel_get_relay_num(1);
            break;
        case 2:
            eraly_gpio = panel_get_relay_num(2);
            break;
        case 3:
            eraly_gpio = panel_get_relay_num(3);
            break;
        default:
            return;
    }
    if (GPIO_IS_VALID(eraly_gpio)) {
        app_ctrl_gpio(eraly_gpio, relay_status);
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
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

typedef struct {
    // 用于处理adc数据
    bool buffer_full;     // 缓冲区填满标志位
    uint8_t buffer_index; // 缓冲区下标
    uint16_t vol;         // 电压值
    uint16_t adc_value_buffer[ADC_VALUE_COUNT];
} adc_value_t;

typedef struct {
    // 设置每个按键的电压范围
    uint16_t min;
    uint16_t max;
} key_vol_t;

typedef struct {     // 用于每个按键的状态
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

typedef struct
{
    bool led_filck;      // 闪烁
    bool key_long_press; // 长按状态
    bool enter_config;   // 进入配置状态
    uint8_t back_status; // 所有背光灯状态(0:关闭,1:打开,2:低亮)
    uint8_t back_status_last;
    uint16_t key_long_count;  // 长按计数
    uint32_t led_filck_count; // 闪烁计数
} common_panel_t;

static common_panel_t my_common_panel;
static adc_value_t my_adc_value;
static panel_status_t my_panel_status[KEY_NUMBER_COUNT] = {0};
static const key_vol_t my_key_vol[KEY_NUMBER_COUNT]     = {

#if defined PANEL_6KEY
    [0] = {.min = 0, .max = 15},
    [1] = {.min = 25, .max = 45},
    [2] = {.min = 85, .max = 110},
    [3] = {.min = 145, .max = 155},
    [4] = {.min = 195, .max = 210},
    [5] = {.min = 230, .max = 255},
#endif
#if defined PANEL_4KEY || defined PANEL_8KEY
    [0] = {.min = 0, .max = 15},
    [1] = {.min = 25, .max = 45},
    [2] = {.min = 85, .max = 110},
    [3] = {.min = 145, .max = 155},
#endif
};

#if defined PANEL_8KEY // 8键采用两组4键的方式
static common_panel_t my_common_panel_ex;
static adc_value_t my_adc_value_ex;
static panel_status_t my_panel_status_ex[KEY_NUMBER_COUNT] = {0};
static const key_vol_t my_key_vol_ex[KEY_NUMBER_COUNT]     = {
    [0] = {.min = 0, .max = 15},
    [1] = {.min = 35, .max = 45},
    [2] = {.min = 100, .max = 110},
    [3] = {.min = 165, .max = 175},
};
#endif

static void panel_gpio_init(void);
static void panel_read_adc(TimerHandle_t xTimer);
static void panel_proce_cmd(TimerHandle_t xTimer);
static void panel_ctrl_led_all(bool led_state, bool is_ex);
static void panel_event_handler(event_type_e event, void *params);
static void panel_data_cb(valid_data_t *data);
static void panel_check_key_status(void);
static void panel_power_status(const panel_cfg_t *cfg, panel_status_t *status);
static void process_cmd_check(valid_data_t *data, const panel_cfg_t *temp_cfg, panel_status_t *temp_status);
static void process_exe_status(const panel_cfg_t *temp_cfg, panel_status_t *temp_status);
static void process_led_flicker(common_panel_t *common_panel, bool is_ex_panel);
static void process_panel_adc(uint16_t vol,
                              const key_vol_t *key_config,
                              panel_status_t *panel_status,
                              common_panel_t *common_panel,
                              adc_value_t *adc_value,
                              bool is_ex_panel);

void panel_key_init(void)
{
    panel_gpio_init();
    adc_channel_t my_adc_channel = {0};
#if defined PANEL_4KEY

#endif

#if defined PANEL_6KEY
    my_adc_channel.adc_channel_0 = true;
    app_adc_init(&my_adc_channel);
    gpio_pin_typedef_t pins[1] = {PA8};
    app_pwm_init(pins, 1);
    app_set_pwm_fade(0, 500, 3000);

#endif

#if defined PANEL_8KEY
    my_adc_channel.adc_channel_0 = true;
    my_adc_channel.adc_channel_1 = true;
    app_adc_init(&my_adc_channel);

    gpio_pin_typedef_t pins[8] = {PA4, PA5, PA6, PA7, PB6, PB7, PB8, PB9};
    app_pwm_init(pins, 8);
    for (uint8_t i = 0; i < 8; i++) { // 上电点亮所有背光灯
        app_set_pwm_fade(i, 500, 3000);
    }
    panel_power_status(app_get_panel_cfg_ex(), my_panel_status_ex);
#endif
    panel_power_status(app_get_panel_cfg(), my_panel_status);

    app_eventbus_subscribe(panel_event_handler); // 订阅事件总线
    // 创建一个静态定时器用于读取adc值
    static StaticTimer_t ReadAdcStaticBuffer;
    static TimerHandle_t ReadAdcTimerHandle = NULL;
    // 创建一个定时器用于处理命令
    static StaticTimer_t PanelStaticBuffer;
    static TimerHandle_t PanleTimerHandle = NULL;

    ReadAdcTimerHandle = xTimerCreateStatic("ReadAdcTimer", 1, true, NULL, panel_read_adc, &ReadAdcStaticBuffer);
    PanleTimerHandle   = xTimerCreateStatic("PanelTimer", 1, true, NULL, panel_proce_cmd, &PanelStaticBuffer);

    if (xTimerStart(PanleTimerHandle, 0) != true || xTimerStart(ReadAdcTimerHandle, 0) != true) {
        // 启动定时器(0表示不阻塞)
        APP_ERROR("timer_start error");
    }
    APP_PRINTF("panel_key_init\n");
}

static void panel_gpio_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);

#if defined PANEL_4KEY

#endif

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

static void panel_read_adc(TimerHandle_t xTimer)
{
    my_adc_value.vol = ADC_TO_VOLTAGE(app_get_adc_value()[0]);
    process_panel_adc(my_adc_value.vol, my_key_vol, my_panel_status, &my_common_panel, &my_adc_value, false);
    process_led_flicker(&my_common_panel, false);

#if defined PANEL_8KEY
    my_adc_value_ex.vol = ADC_TO_VOLTAGE(app_get_adc_value()[1]);
    process_panel_adc(my_adc_value_ex.vol, my_key_vol_ex, my_panel_status_ex, &my_common_panel_ex, &my_adc_value_ex, true);
    process_led_flicker(&my_common_panel_ex, true);
#endif
}
static void process_panel_adc(uint16_t vol,
                              const key_vol_t *key_config,
                              panel_status_t *panel_status,
                              common_panel_t *common_panel,
                              adc_value_t *adc_value,
                              bool is_ex_panel)
{
    for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
        if (vol >= key_config[i].min && vol <= key_config[i].max) {
            // 填充满buffer才会执行下面的代码
            adc_value->adc_value_buffer[adc_value->buffer_index++] = vol;
            if (adc_value->buffer_index >= ADC_VALUE_COUNT) {
                adc_value->buffer_index = 0;
                adc_value->buffer_full  = 1;
                uint16_t new_value      = app_calculate_average(adc_value->adc_value_buffer, ADC_VALUE_COUNT);
                if (new_value >= key_config[i].min && new_value <= key_config[i].max) {
                    if (panel_status[i].key_press == false) {
                        if (common_panel->enter_config == false) {
                            panel_status[i].key_status = !panel_status[i].key_status;
                            if (panel_status[i].relay_short != false) {
                                app_send_cmd(i, 0x00, 0xF1, 0x62, is_ex_panel);
                            } else {
                                app_send_cmd(i, panel_status[i].key_status, 0xF1, 0x00, is_ex_panel);
                            }
                            panel_status[i].key_press = true;
                        }
                        common_panel->key_long_press = true;
                        common_panel->key_long_count = 0;
                    } else if (common_panel->key_long_press == true) {
                        common_panel->key_long_count++;
                        if (common_panel->key_long_count >= 500) {
                            app_send_cmd(0, 0, 0xF8, 0x00, is_ex_panel);
                            common_panel->key_long_press = false;
                        }
                    }
                }
            }
        } else if (vol >= DEFAULT_MIN_VALUE && vol <= DEFAULT_MAX_VALUE) {
            panel_status[i].key_press    = false;
            common_panel->key_long_press = false;
            common_panel->key_long_count = 0;
        }
    }
}

static void process_led_flicker(common_panel_t *common_panel, bool is_ex_panel)
{
    if (common_panel->led_filck == true) {
        common_panel->led_filck_count++;
        if (common_panel->led_filck_count <= 500) {
            panel_ctrl_led_all(true, is_ex_panel);
        } else if (common_panel->led_filck_count <= 1000) {
            panel_ctrl_led_all(false, is_ex_panel);
        } else {
            common_panel->led_filck_count = 0;
            common_panel->led_filck       = false;
            panel_ctrl_led_all(true, is_ex_panel);
        }
    }
}

static void panel_proce_cmd(TimerHandle_t xTimer)
{
    const panel_cfg_t *temp_cfg = app_get_panel_cfg();
    process_exe_status(temp_cfg, my_panel_status);
#if defined PANEL_8KEY
    const panel_cfg_t *temp_cfg_ex = app_get_panel_cfg_ex();
    process_exe_status(temp_cfg_ex, my_panel_status_ex);
#endif
}

static void process_exe_status(const panel_cfg_t *temp_cfg, panel_status_t *temp_status)
{
    for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {

        if (temp_status[i].led_short) { // led 短亮
            if (temp_status[i].led_short_count == 0) {
                temp_status[i].led_open = true;
            }
            temp_status[i].led_short_count++;
            if (temp_status[i].led_short_count >= SHORT_TIME_LED) {
                temp_status[i].led_open        = false;
                temp_status[i].led_short       = false;
                temp_status[i].led_short_count = 0;
            }
        }

        if (temp_status[i].relay_short == true) { // 继电器导通30s
            if (temp_status[i].relay_open_count == 0) {
                temp_status[i].relay_open = true;
            }
            temp_status[i].relay_open_count++;
            if (temp_status[i].relay_open_count >= SHORT_TIME_RELAY) {
                temp_status[i].relay_open       = false;
                temp_status[i].relay_short      = false;
                temp_status[i].relay_open_count = 0;
            }
        }

        if (temp_status[i].led_open != temp_status[i].led_last) { // 开关led
            APP_SET_GPIO(temp_cfg[i].key_led, temp_status[i].led_open);
            temp_status[i].led_last = temp_status[i].led_open;
            panel_check_key_status(); // 每次led变化后都会检测一次
        }

        if (temp_status[i].relay_open != temp_status[i].relay_last) { // 开关继电器
            // 开关该按键所控的所有继电器
            APP_SET_GPIO(temp_cfg[i].key_relay[0], temp_status[i].relay_open);
            APP_SET_GPIO(temp_cfg[i].key_relay[1], temp_status[i].relay_open);
            APP_SET_GPIO(temp_cfg[i].key_relay[2], temp_status[i].relay_open);
            APP_SET_GPIO(temp_cfg[i].key_relay[3], temp_status[i].relay_open);
            // APP_PRINTF("%d%d%d%d\n", APP_GET_GPIO(PB12), APP_GET_GPIO(PB13), APP_GET_GPIO(PB14), APP_GET_GPIO(PB15));
            temp_status[i].relay_last = temp_status[i].relay_open;

            temp_status[i].relay_open_count = 0;
        }
#if defined PANEL_6KEY // 6键面板与8键的背光灯开关逻辑不同,在此用宏定义隔开
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

static void panel_check_key_status(void)
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

static void panel_data_cb(valid_data_t *data)
{
    const panel_cfg_t *temp_cfg = app_get_panel_cfg();
    process_cmd_check(data, temp_cfg, my_panel_status);

#if defined PANEL_8KEY
    const panel_cfg_t *temp_cfg_ex = app_get_panel_cfg_ex();
    process_cmd_check(data, temp_cfg_ex, my_panel_status_ex);
#endif
}

static void process_cmd_check(valid_data_t *data, const panel_cfg_t *temp_cfg, panel_status_t *temp_status)
{
    switch (data->data[1]) {

        case 0x00: { // 总关
            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
                if ((temp_cfg[i].key_area >> 4) == (data->data[4] >> 4) ||
                    (data->data[4] >> 4) == 0xF) {         // 匹配"总关区域"(若为0xF则可以控制所有总关区域)
                    if (temp_cfg[i].key_func == 0x0F) {    // 匹配"夜灯"
                        if (temp_cfg[i].key_perm & 0x20) { // "夜灯"受"总关"控制
                            temp_status[i].key_status = data->data[2];
                            temp_status[i].led_open   = data->data[2];
                            temp_status[i].relay_open = data->data[2];
                        }
                    } else if (temp_cfg[i].key_func == 0x0D ||
                               temp_cfg[i].key_func == 0x0E ||
                               temp_cfg[i].key_func == 0x01) { // 匹配"场景""灯控""总关"

                        if (temp_cfg[i].key_perm & 0x20) { // 受"总关"控制
                            temp_status[i].key_status = false;
                            temp_status[i].led_open   = false;
                            temp_status[i].relay_open = false;
                        }
                    }
                    if (temp_cfg[i].key_func == data->data[1] &&
                        temp_cfg[i].key_group == data->data[3] &&
                        temp_cfg[i].key_group != 0x00) { // 匹配双控

                        temp_status[i].led_short  = true;
                        temp_status[i].relay_open = data->data[2];
                        temp_status[i].key_status = data->data[2];
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
                            temp_status[i].key_status = !data->data[2];
                            temp_status[i].led_open   = !data->data[2];
                            temp_status[i].relay_open = !data->data[2];
                        }
                    } else if (temp_cfg[i].key_func == 0x0D ||
                               temp_cfg[i].key_func == 0x0E ||
                               temp_cfg[i].key_func == 0x01) {

                        if (temp_cfg[i].key_perm & 0x01) {
                            temp_status[i].key_status = data->data[2];
                            temp_status[i].led_open   = data->data[2];
                            temp_status[i].relay_open = data->data[2];
                        }
                    }
                    if (temp_cfg[i].key_func == 0x00) {
                        // 匹配到总关(短亮)
                        temp_status[i].led_short  = true;
                        temp_status[i].relay_open = data->data[2];
                        temp_status[i].key_status = data->data[2];
                    }
                    if (temp_cfg[i].key_func == data->data[1] &&
                        temp_cfg[i].key_group == data->data[3] &&
                        temp_cfg[i].key_group != 0x00) {
                        // 匹配双控
                        temp_status[i].led_open   = data->data[2];
                        temp_status[i].relay_open = data->data[2];
                        temp_status[i].key_status = data->data[2];
                    }
                }
            }

        } break;
        case 0x02: { // 清理房间
            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
                if (temp_cfg[i].key_func == data->data[1] &&
                    temp_cfg[i].key_group == data->data[3] &&
                    temp_cfg[i].key_group != 0x00) {
                    temp_status[i].led_open   = data->data[2];
                    temp_status[i].relay_open = data->data[2];
                    temp_status[i].key_status = data->data[2];
                    if (data->data[2] == true) { // 开启"清理房间"关闭"请勿打扰"
                        for (uint8_t j = 0; j < KEY_NUMBER_COUNT; j++) {
                            if (temp_cfg[j].key_func == 0x03) {
                                temp_status[j].led_open   = false;
                                temp_status[j].relay_open = false;
                                temp_status[j].key_status = false;
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
                    temp_status[i].led_open   = data->data[2];
                    temp_status[i].relay_open = data->data[2];
                    temp_status[i].key_status = data->data[2];
                    if (data->data[2] == true) { // 开启"请勿打扰"关闭"清理房间"
                        for (uint8_t j = 0; j < KEY_NUMBER_COUNT; j++) {
                            if (temp_cfg[j].key_func == 0x02) {
                                temp_status[j].led_open   = false;
                                temp_status[j].relay_open = false;
                                temp_status[j].key_status = false;
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
                    temp_status[i].led_open   = data->data[2];
                    temp_status[i].relay_open = data->data[2];
                    temp_status[i].key_status = data->data[2];
                }
            }

        } break;
        case 0x05: { // 退房
            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
                if (temp_cfg[i].key_func == data->data[1] &&
                    temp_cfg[i].key_group == data->data[3] &&
                    temp_cfg[i].key_group != 0x00) {
                    temp_status[i].led_open   = data->data[2];
                    temp_status[i].relay_open = data->data[2];
                    temp_status[i].key_status = data->data[2];
                }
            }

        } break;
        case 0x06: { // 紧急呼叫
            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
                if (temp_cfg[i].key_func == data->data[1] &&
                    temp_cfg[i].key_group == data->data[3] &&
                    temp_cfg[i].key_group != 0x00) {
                    temp_status[i].led_open   = data->data[2];
                    temp_status[i].relay_open = data->data[2];
                    temp_status[i].key_status = data->data[2];
                }
            }

        } break;
        case 0x07: { // 请求服务
            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
                if (temp_cfg[i].key_func == data->data[1] &&
                    temp_cfg[i].key_group == data->data[3] &&
                    temp_cfg[i].key_group != 0x00) {
                    temp_status[i].led_open   = data->data[2];
                    temp_status[i].relay_open = data->data[2];
                    temp_status[i].key_status = data->data[2];
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
                        if (temp_cfg[j].key_func == 0x11 &&
                            temp_cfg[j].key_group == data->data[3]) {
                            temp_status[j].relay_open  = false;
                            temp_status[j].relay_short = false;
                        }
                    }
                    temp_status[i].led_short   = true; // 短亮
                    temp_status[i].relay_short = true; // 继电器导通 30s
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
                        if (temp_cfg[j].key_func == 0x10 &&
                            temp_cfg[j].key_group == data->data[3]) {
                            temp_status[j].relay_open  = false;
                            temp_status[j].relay_short = false;
                        }
                    }
                    temp_status[i].led_short   = true;
                    temp_status[i].relay_short = true;
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
                                temp_status[j].led_open   = false;
                                temp_status[j].relay_open = false;
                                temp_status[j].key_status = false;
                            }
                        }
                        temp_status[i].led_open   = data->data[2];
                        temp_status[i].relay_open = data->data[2];
                        temp_status[i].key_status = data->data[2];
                    }
                }
            }

        } break;
        case 0x0E: { // 灯控模式

            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
                if (temp_cfg[i].key_func == data->data[1] &&
                    temp_cfg[i].key_group == data->data[3] &&
                    temp_cfg[i].key_group != 0x00) {

                    temp_status[i].led_open   = data->data[2];
                    temp_status[i].relay_open = data->data[2];
                    temp_status[i].key_status = data->data[2];
                }
            }

        } break;
        case 0x0F: { // 夜灯
            for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
                if (temp_cfg[i].key_func == data->data[1] &&
                    temp_cfg[i].key_group == data->data[3] &&
                    temp_cfg[i].key_group != 0x00) {
                    temp_status[i].led_open   = data->data[2];
                    temp_status[i].relay_open = data->data[2];
                    temp_status[i].key_status = data->data[2];
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
                    temp_status[i].relay_short == true) { // 匹配分组后如果该继电器正在动作才能触发暂停
                    temp_status[i].led_short   = true;
                    temp_status[i].relay_short = false;
                    temp_status[i].relay_open  = false;
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

static void panel_ctrl_led_all(bool led_state, bool is_ex)
{
    panel_status_t *temp = NULL;
#ifndef PANEL_8KEY
    temp = my_panel_status;
#endif
#if defined PANEL_8KEY
    temp = is_ex ? my_panel_status_ex : my_panel_status;
#endif
    for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
        temp[i].led_open = led_state;
    }
}

static void panel_event_handler(event_type_e event, void *params)
{
    switch (event) {
        case EVENT_ENTER_CONFIG: {
            panel_ctrl_led_all(true, false);
            my_common_panel.enter_config = true;
        } break;
        case EVENT_EXIT_CONFIG: {
            panel_ctrl_led_all(false, false);
            my_common_panel.enter_config = false;
            NVIC_SystemReset(); // 退出"配置模式"后触发软件复位
        } break;
        case EVENT_SAVE_SUCCESS: {
            my_common_panel.led_filck = true;
        } break;
#if defined PANEL_8KEY
        case EVENT_ENTER_CONFIG_EX:
            panel_ctrl_led_all(true, true);
            my_common_panel_ex.enter_config = true;
            break;
        case EVENT_EXIT_CONFIG_EX: {
            panel_ctrl_led_all(false, true);
            my_common_panel_ex.enter_config = false;
            NVIC_SystemReset();
        } break;
        case EVENT_SAVE_SUCCESS_EX: {
            my_common_panel_ex.led_filck = true;
        } break;
#endif
        case EVENT_RECEIVE_CMD: {
            valid_data_t *valid_data = (valid_data_t *)params;
            panel_data_cb(valid_data);
        } break;
        default:
            return;
    }
}

static void panel_power_status(const panel_cfg_t *cfg, panel_status_t *status)
{
    for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
        if (cfg[i].key_perm & 0x08) { // 权限是否勾选"迎宾"
            status[i].led_open   = true;
            status[i].relay_open = true;
            status[i].key_status = true;
        }
    }
}

#endif
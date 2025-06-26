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
#include "../usart/usart.h"
#include "../base/panel_base.h"
#include "../eventbus/eventbus.h"
#include "../protocol/protocol.h"
#include "../pwm/pwm.h"
#include "../config/config.h"

#if defined PANEL_KEY

    #define ADC_TO_VOLTAGE(adc_val) ((adc_val) * 330 / 4096) // adc值转电压

    #define ADC_VALUE_COUNT         10 // 电压值缓冲区数量

    #define DEFAULT_MIN_VALUE       329 // 无按键按下时的最小电压值
    #define DEFAULT_MAX_VALUE       330 // 无按键按下时的最大电压值

    #define SHORT_TIME_LED          100   // 短亮时间
    #define SHORT_TIME_RELAY        30000 // 继电器导通时间

typedef struct {
    // 用于处理adc数据
    bool buf_full;   // 缓冲区填满标志位
    uint8_t buf_idx; // 缓冲区下标
    uint16_t vol;    // 电压值
    uint16_t vol_buf[ADC_VALUE_COUNT];
} adc_value_t;

typedef struct {
    uint16_t min;
    uint16_t max;
} key_vol_t;

typedef struct { // 用于每个按键的状态

    bool key_press;  // 按键按下
    bool key_status; // 按键状态

    bool led_w_open;  // 白灯状态
    bool led_w_short; // 启用短亮(窗帘开关)
    bool led_w_last;  // 白灯上次状态

    bool led_y_open; // 黄灯状态
    bool led_y_last; // 黄灯上次状态

    bool relay_open;  // 继电状态
    bool relay_short; // 继电器短开
    bool relay_last;  // 继电器上次状态

    uint16_t led_w_short_count; // 短亮计数器
    uint32_t relay_open_count;  // 继电器导通计数器
    key_vol_t vol_range;        // 按键电压范围

} panel_status_t;

typedef struct
{
    bool led_filck;      // 闪烁
    bool key_long_press; // 长按状态
    bool enter_config;   // 进入配置状态
    uint8_t back_status; // 所有背光灯状态(0:关闭,1:打开,2:低亮)
    uint8_t back_status_last;
    uint16_t key_long_count;  // 长按计数
    uint16_t led_filck_count; // 闪烁计数
} common_panel_t;

static common_panel_t my_common_panel;
static adc_value_t my_adc_value;
static panel_status_t my_panel_status[KEY_NUMBER] = {

    #if defined PANEL_6KEY
    [0] = {.vol_range = {0, 15}},
    [1] = {.vol_range = {25, 45}},
    [2] = {.vol_range = {85, 110}},
    [3] = {.vol_range = {145, 155}},
    [4] = {.vol_range = {195, 210}},
    [5] = {.vol_range = {230, 255}},
    #endif

    #if defined PANEL_8KEY
    [0] = {.vol_range = {0, 15}},
    [1] = {.vol_range = {25, 45}},
    [2] = {.vol_range = {85, 110}},
    [3] = {.vol_range = {145, 155}},
    #endif
};

    #if defined PANEL_8KEY // 8键采用两组4键的方式
static common_panel_t my_common_panel_ex;
static adc_value_t my_adc_value_ex;
static panel_status_t my_panel_status_ex[KEY_NUMBER] = {
    [0] = {.vol_range = {0, 15}},
    [1] = {.vol_range = {25, 45}},
    [2] = {.vol_range = {85, 110}},
    [3] = {.vol_range = {165, 175}},
};

    #endif

// 函数声明
static void panel_gpio_init(void);
static void panel_power_status(void);
static void panel_check_key_status(void);
static void panel_data_cb(valid_data_t *data);
static void panel_read_adc(TimerHandle_t xTimer);
static void panel_proce_cmd(TimerHandle_t xTimer);
static void panel_ctrl_led_all(bool led_state, bool is_ex);
static void panel_event_handler(event_type_e event, void *params);
static void process_led_flicker(common_panel_t *common_panel, bool is_ex_panel);
static void panel_fast_exe(panel_status_t *temp_fast, uint8_t flag);
static void process_exe_status(const panel_cfg_t *temp_cfg, panel_status_t *temp_status);
static void process_cmd_check(valid_data_t *data, const panel_cfg_t *temp_cfg, panel_status_t *temp_status);
static void process_panel_adc(panel_status_t *panel_status, common_panel_t *common_panel, adc_value_t *adc_value, bool is_ex);

void panel_key_init(void)
{
    panel_gpio_init();
    adc_channel_t my_adc_channel = {0};
    app_pwm_init(); // 初始化 PWM 模块
    #if defined PANEL_4KEY

    #endif

    #if defined PANEL_6KEY
    my_adc_channel.adc_channel_0 = true;
    app_adc_init(&my_adc_channel);

    app_pwm_add_pin(PA8);
    app_set_pwm_fade(PA8, 500, 3000);
    #endif

    #if defined PANEL_8KEY
    my_adc_channel.adc_channel_0 = true;
    my_adc_channel.adc_channel_1 = true;
    app_adc_init(&my_adc_channel);

    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
        app_pwm_add_pin(app_get_panel_cfg()[i].led_y_pin);
        app_pwm_add_pin(app_get_panel_cfg_ex()[i].led_y_pin);
    };

    for (uint8_t i = 0; i < KEY_NUMBER; i++) { // 上电点亮所有背光灯
        app_set_pwm_fade(app_get_panel_cfg()[i].led_y_pin, 500, 3000);
        app_set_pwm_fade(app_get_panel_cfg_ex()[i].led_y_pin, 500, 3000);
    }
    #endif
    panel_power_status();

    app_eventbus_subscribe(panel_event_handler); // 订阅事件总线
    // 创建一个静态定时器用于读取adc值
    static StaticTimer_t ReadAdcStaticBuffer;
    static TimerHandle_t ReadAdcTimerHandle = NULL;
    // 创建一个定时器用于处理命令
    static StaticTimer_t PanelStaticBuffer;
    static TimerHandle_t PanleTimerHandle = NULL;

    // 集中处理adc
    ReadAdcTimerHandle = xTimerCreateStatic("ReadAdcTimer", 1, true, NULL, panel_read_adc, &ReadAdcStaticBuffer);
    // 集中状态机轮询
    PanleTimerHandle = xTimerCreateStatic("PanelTimer", 1, true, NULL, panel_proce_cmd, &PanelStaticBuffer);

    if (xTimerStart(PanleTimerHandle, 0) != true || xTimerStart(ReadAdcTimerHandle, 0) != true) {
        // 启动定时器(0表示不阻塞)
        APP_ERROR("timer_start error");
    }
    APP_PRINTF("panel_key_init[%d]\n", KEY_NUMBER);
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
    // 8 个黄灯
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
    // 4 s个继电器
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_12);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_13);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_14);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_15);
    #endif
}

static void panel_read_adc(TimerHandle_t xTimer)
{
    my_adc_value.vol = ADC_TO_VOLTAGE(app_get_adc_value()[0]);
    process_panel_adc(my_panel_status, &my_common_panel, &my_adc_value, false);
    if (my_common_panel.led_filck == true) {
        process_led_flicker(&my_common_panel, false);
    }
    #if defined PANEL_8KEY
    my_adc_value_ex.vol = ADC_TO_VOLTAGE(app_get_adc_value()[1]);
    process_panel_adc(my_panel_status_ex, &my_common_panel_ex, &my_adc_value_ex, true);
    if (my_common_panel_ex.led_filck == true) {
        process_led_flicker(&my_common_panel_ex, true);
    }
    #endif
}

static void process_panel_adc(panel_status_t *panel_status, common_panel_t *common_panel, adc_value_t *adc_value, bool is_ex)
{
    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
        if (adc_value->vol >= panel_status[i].vol_range.min && adc_value->vol <= panel_status[i].vol_range.max) {
            // 填充满buffer才会执行下面的代码
            adc_value->vol_buf[adc_value->buf_idx++] = adc_value->vol;
            if (adc_value->buf_idx >= ADC_VALUE_COUNT) {
                adc_value->buf_idx  = 0;
                adc_value->buf_full = 1;
                uint16_t new_value  = app_calculate_average(adc_value->vol_buf, ADC_VALUE_COUNT);
                if (new_value >= panel_status[i].vol_range.min && new_value <= panel_status[i].vol_range.max) {
                    if (panel_status[i].key_press == false) {
                        if (common_panel->enter_config == false) {
                            panel_status[i].key_status = !panel_status[i].key_status;
                            if (panel_status[i].relay_short != false) { // 有继电器正在短开,发送特殊命令
                                app_send_cmd(i, 0x00, PANEL_HEAD, SPECIAL_CMD, is_ex);
                            } else {
                                app_send_cmd(i, panel_status[i].key_status, PANEL_HEAD, COMMON_CMD, is_ex);
                            }
                            panel_status[i].key_press = true;
                        }
                        common_panel->key_long_press = true;
                        common_panel->key_long_count = 0;
                    } else if (common_panel->key_long_press == true) {
                        common_panel->key_long_count++;
                        if (common_panel->key_long_count >= 500) {
                            app_send_cmd(0, 0, APPLY_CONFIG, 0x00, is_ex);
                            common_panel->key_long_press = false;
                        }
                    }
                }
            }
        } else if (adc_value->vol >= DEFAULT_MIN_VALUE && adc_value->vol <= DEFAULT_MAX_VALUE) {
            panel_status[i].key_press    = false;
            common_panel->key_long_press = false;
            common_panel->key_long_count = 0;
        }
    }
}

static void process_led_flicker(common_panel_t *common_panel, bool is_ex_panel)
{
    common_panel->led_filck_count++;
    if (common_panel->led_filck_count <= 500) {
        panel_ctrl_led_all(false, is_ex_panel);
    } else if (common_panel->led_filck_count <= 1000) {
        panel_ctrl_led_all(true, is_ex_panel);
    } else if (common_panel->led_filck_count <= 1500) {
        panel_ctrl_led_all(false, is_ex_panel);
    } else {
        common_panel->led_filck_count = 0;
        common_panel->led_filck       = false;
        panel_ctrl_led_all(true, is_ex_panel);
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
    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
        panel_status_t *p_status = &temp_status[i];
        const panel_cfg_t *p_cfg = &temp_cfg[i];

        if (p_status->led_w_short) { // 白灯短亮逻辑
            if (p_status->led_w_short_count++ == 0) {
                p_status->led_w_open = true;
            }
            if (p_status->led_w_short_count >= SHORT_TIME_LED) {
                p_status->led_w_open        = false;
                p_status->led_w_short       = false;
                p_status->led_w_short_count = 0;
            }
        }
        if (p_status->relay_short) { // 继电器短开逻辑
            if (p_status->relay_open_count++ == 0) {
                p_status->relay_open = true;
            }
            if (p_status->relay_open_count >= SHORT_TIME_RELAY) {
                p_status->relay_open       = false;
                p_status->relay_short      = false;
                p_status->led_w_open       = false;
                p_status->key_status       = false;
                p_status->relay_open_count = 0;
            }
        }
        if (p_status->led_w_open != p_status->led_w_last) { // 白灯状态更新
            APP_SET_GPIO(p_cfg->led_w_pin, p_status->led_w_open);
            app_set_pwm_fade(p_cfg->led_y_pin, p_status->led_w_open ? 0 : 500, 1000);
            p_status->led_w_last = p_status->led_w_open;
            panel_check_key_status(); // 每次led变化后都会检测一次
        }

        if (p_status->led_y_open != p_status->led_y_last) { // 黄灯状态更新
            app_set_pwm_fade(p_cfg->led_y_pin, p_status->led_y_open ? 500 : 0, 3000);
            p_status->led_y_last = p_status->led_y_open;
        }

        if (p_status->relay_open != p_status->relay_last) { // 继电器状态更新
            APP_SET_GPIO(p_cfg->relay_pin[0], p_status->relay_open);
            APP_SET_GPIO(p_cfg->relay_pin[1], p_status->relay_open);
            APP_SET_GPIO(p_cfg->relay_pin[2], p_status->relay_open);
            APP_SET_GPIO(p_cfg->relay_pin[3], p_status->relay_open);

            p_status->relay_last       = p_status->relay_open;
            p_status->relay_open_count = 0;
        }
    #if defined PANEL_6KEY // 6键面板与8键的背光灯开关逻辑不同,在此用宏定义隔开
        if (my_common_panel.back_status != my_common_panel.back_status_last) {
            if (my_common_panel.back_status == 0) {
                app_set_pwm_fade(PA8, 0, 3000);
            } else if (my_common_panel.back_status == 1) {
                app_set_pwm_fade(PA8, 500, 3000);
            } else if (my_common_panel.back_status == 2) {
                app_set_pwm_fade(PA8, 50, 3000);
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
    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
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
    APP_PRINTF_BUF("[RECV]", data->data, data->length);
    const panel_cfg_t *temp_cfg = app_get_panel_cfg();
    process_cmd_check(data, temp_cfg, my_panel_status);

    #if defined PANEL_8KEY
    const panel_cfg_t *temp_cfg_ex = app_get_panel_cfg_ex();
    process_cmd_check(data, temp_cfg_ex, my_panel_status_ex);
    #endif
}

static void process_cmd_check(valid_data_t *data, const panel_cfg_t *temp_cfg, panel_status_t *temp_status)
{
    uint8_t cmd   = data->data[1];
    uint8_t sw    = data->data[2];
    uint8_t group = data->data[3];
    uint8_t area  = data->data[4];
    switch (cmd) {
        case ALL_CLOSE: { // 总关
            for (uint8_t i = 0; i < KEY_NUMBER; i++) {
                const panel_cfg_t *p_cfg = &temp_cfg[i];
                panel_status_t *p_status = &temp_status[i];
                if ((BIT5(p_cfg->perm) == true) && // "睡眠"被勾选,"总开关分区"相同 或 0xF
                    ((H_BIT(area) == H_BIT(p_cfg->area)) || (H_BIT(area) == 0xF))) {
                    switch (p_cfg->func) {
                        case ALL_CLOSE:
                            if (group == p_cfg->group) {
                                panel_fast_exe(p_status, (0b00011010 & ~0x01) | (sw & 0x01));
                            }
                            break;
                        case NIGHT_LIGHT:
                        case DND_MODE:
                            panel_fast_exe(p_status, (0b00010110 & ~0x01) | (sw & 0x01));
                            break;
                        case SCENE_MODE:
                        case LIGHT_MODE:
                        case ALL_ON_OFF:
                        case CLEAN_ROOM:
                            panel_fast_exe(p_status, (0b00010110 & ~0x01) | 0x00);
                            break;
                        default:
                            break;
                    }
                }
            }
        } break;
        case ALL_ON_OFF: { // 总开关
            for (uint8_t i = 0; i < KEY_NUMBER; i++) {
                const panel_cfg_t *p_cfg = &temp_cfg[i];
                panel_status_t *p_status = &temp_status[i];
                if ((BIT5(p_cfg->perm) == true) && // "总开关"被勾选,"总开关分区"相同 或 0xF
                    ((H_BIT(area) == H_BIT(p_cfg->area)) || (H_BIT(area) == 0xF))) {
                    switch (p_cfg->func) {
                        case ALL_ON_OFF:
                            if (group == p_cfg->group) {
                                panel_fast_exe(p_status, (0b00010110 & ~0x01) | (sw & 0x01));
                            }
                            break;
                        case NIGHT_LIGHT:
                        case DND_MODE:
                            panel_fast_exe(p_status, (0b00010110 & ~0x01) | (~sw & 0x01));
                            break;
                        case SCENE_MODE:
                        case LIGHT_MODE:
                        case CLEAN_ROOM:
                            panel_fast_exe(p_status, (0b00010110 & ~0x01) | (sw & 0x01));
                            break;
                        case ALL_CLOSE:
                            panel_fast_exe(p_status, (0b00011010 & ~0x01) | (sw & 0x01));
                            break;
                        default:
                            break;
                    }
                }
            }
        } break;
        case CLEAN_ROOM: { // 清理房间
            for (uint8_t i = 0; i < KEY_NUMBER; i++) {
                const panel_cfg_t *p_cfg = &temp_cfg[i];
                panel_status_t *p_status = &temp_status[i];
                if (p_cfg->func == CLEAN_ROOM &&
                    (group == p_cfg->group || group == 0xFF)) {
                    if (sw == true) {
                        for (uint8_t j = 0; j < KEY_NUMBER; j++) {
                            const panel_cfg_t *p_cfg_j = &temp_cfg[j];
                            panel_status_t *p_status_j = &temp_status[j];
                            if (p_cfg_j->func == DND_MODE &&
                                (group == p_cfg_j->group || group == 0xFF)) {
                                panel_fast_exe(p_status_j, (0b00010110 & ~0x01) | (~sw & 0x01));
                            }
                        }
                    }
                    panel_fast_exe(p_status, (0b00010110 & ~0x01) | (sw & 0x01));
                }
            }
        } break;
        case DND_MODE: { // 勿扰模式
            for (uint8_t i = 0; i < KEY_NUMBER; i++) {
                const panel_cfg_t *p_cfg = &temp_cfg[i];
                panel_status_t *p_status = &temp_status[i];
                if (p_cfg->func == DND_MODE &&
                    (group == p_cfg->group || group == 0xFF)) {
                    if (sw == true) {
                        for (uint8_t j = 0; j < KEY_NUMBER; j++) {
                            const panel_cfg_t *p_cfg_j = &temp_cfg[j];
                            panel_status_t *p_status_j = &temp_status[j];
                            if (p_cfg_j->func == CLEAN_ROOM &&
                                (group == p_cfg_j->group || group == 0xFF)) {
                                panel_fast_exe(p_status_j, (0b00010110 & ~0x01) | (~sw & 0x01));
                            }
                        }
                    }
                    panel_fast_exe(&temp_status[i], (0b00010110 & ~0x01) | (sw & 0x01));
                }
            }
        } break;
        case LATER_MODE: { // 请稍后
            for (uint8_t i = 0; i < KEY_NUMBER; i++) {
                const panel_cfg_t *p_cfg = &temp_cfg[i];
                panel_status_t *p_status = &temp_status[i];
                if (p_cfg->func == LATER_MODE &&
                    (group == p_cfg->group || group == 0xFF)) {
                    panel_fast_exe(p_status, (0b00100110 & ~0x01) | (sw & 0x01));
                }
            }
        } break;
        case CHECK_OUT: { // 退房
            for (uint8_t i = 0; i < KEY_NUMBER; i++) {
                const panel_cfg_t *p_cfg = &temp_cfg[i];
                panel_status_t *p_status = &temp_status[i];
                if (p_cfg->func == CHECK_OUT &&
                    (group == p_cfg->group || group == 0xFF)) {
                    panel_fast_exe(p_status, (0b00010110 & ~0x01) | (sw & 0x01));
                }
            }
        } break;
        case SOS_MODE: { // 紧急呼叫
            for (uint8_t i = 0; i < KEY_NUMBER; i++) {
                const panel_cfg_t *p_cfg = &temp_cfg[i];
                panel_status_t *p_status = &temp_status[i];
                if (p_cfg->func == SOS_MODE &&
                    (group == p_cfg->group || group == 0xFF)) {
                    panel_fast_exe(p_status, (0b00010110 & ~0x01) | (sw & 0x01));
                }
            }
        } break;
        case SERVICE: { // 请求服务
            for (uint8_t i = 0; i < KEY_NUMBER; i++) {
                const panel_cfg_t *p_cfg = &temp_cfg[i];
                panel_status_t *p_status = &temp_status[i];
                if (p_cfg->func == SERVICE &&
                    (group == p_cfg->group || group == 0xFF)) {
                    panel_fast_exe(p_status, (0b00010110 & ~0x01) | (sw & 0x01));
                }
            }
        } break;
        case CURTAIN_OPEN: { // 窗帘开
            for (uint8_t i = 0; i < KEY_NUMBER; i++) {
                const panel_cfg_t *p_cfg = &temp_cfg[i];
                panel_status_t *p_status = &temp_status[i];
                if (p_cfg->func == CURTAIN_OPEN &&
                    (group == p_cfg->group || group == 0xFF)) {
                    for (uint8_t j = 0; j < KEY_NUMBER; j++) { // 先关闭同分组的"窗帘关"
                        const panel_cfg_t *p_cfg_j = &temp_cfg[j];
                        panel_status_t *p_status_j = &temp_status[j];
                        if (p_cfg_j->func == CURTAIN_CLOSE &&
                            (group == p_cfg_j->group || group == 0xFF)) {
                            panel_fast_exe(p_status_j, (0b00110000 & ~0x01) | 0x00);
                        }
                    }
                    panel_fast_exe(p_status, (0b00101010 & ~0x01) | 0x01);
                }
            }
        } break;
        case CURTAIN_CLOSE: { // 窗帘关
            for (uint8_t i = 0; i < KEY_NUMBER; i++) {
                const panel_cfg_t *p_cfg = &temp_cfg[i];
                panel_status_t *p_status = &temp_status[i];
                if (p_cfg->func == CURTAIN_CLOSE &&
                    (group == p_cfg->group || group == 0xFF)) {
                    for (uint8_t j = 0; j < KEY_NUMBER; j++) { // 先关闭同分组的"窗帘开"
                        const panel_cfg_t *p_cfg_j = &temp_cfg[j];
                        panel_status_t *p_status_j = &temp_status[j];
                        if (p_cfg_j->func == CURTAIN_OPEN &&
                            (group == p_cfg_j->group || group == 0xFF)) {
                            panel_fast_exe(p_status_j, (0b00110000 & ~0x01) | 0x00);
                        }
                    }
                    panel_fast_exe(p_status, (0b00101010 & ~0x01) | 0x01);
                }
            }
        } break;
        case SCENE_MODE: { // 场景模式
            for (uint8_t i = 0; i < KEY_NUMBER; i++) {
                const panel_cfg_t *p_cfg = &temp_cfg[i];
                panel_status_t *p_status = &temp_status[i];
                if ((p_cfg->scene_group != 0x00) && // 屏蔽掉没有勾选任何场景分组的按键
                    ((L_BIT(area) == L_BIT(p_cfg->area)) || (L_BIT(area) == 0xF))) {
                    panel_fast_exe(p_status, (0b00010110 & ~0x01) | 0x00); // 先关闭该场景区域的所有分组
                    uint8_t mask = data->data[7] & p_cfg->scene_group;     // 找出两个字节中同为1的位
                    if (mask != 0) {                                       // 任意一位同为1,说明勾选了该场景分组,执行动作
                        panel_fast_exe(p_status, (0b00010110 & ~0x01) | (sw & 0x01));
                    }
                }
            }
        } break;
        case LIGHT_MODE: { // 灯控模式
            for (uint8_t i = 0; i < KEY_NUMBER; i++) {
                const panel_cfg_t *p_cfg = &temp_cfg[i];
                panel_status_t *p_status = &temp_status[i];
                if (p_cfg->func == LIGHT_MODE &&
                    (group == p_cfg->group || group == 0xFF)) {
                    panel_fast_exe(p_status, (0b00010110 & ~0x01) | (sw & 0x01));
                }
            }

        } break;
        case NIGHT_LIGHT: { // 夜灯
            for (uint8_t i = 0; i < KEY_NUMBER; i++) {
                const panel_cfg_t *p_cfg = &temp_cfg[i];
                panel_status_t *p_status = &temp_status[i];
                if (p_cfg->func == NIGHT_LIGHT &&
                    (group == p_cfg->group || group == 0xFF)) {
                    panel_fast_exe(p_status, (0b00010110 & ~0x01) | (sw & 0x01));
                }
            }
        } break;
        case LIGHT_UP:
            break;
        case LIGHT_DOWN:
            break;
        case DIMMING_3:
            break;
        case DIMMING_4:
            break;
        case UNLOCKING:
            break;
        case BLUETOOTH:
            break;
        case CURTAIN_STOP: { // 窗帘停
            for (uint8_t i = 0; i < KEY_NUMBER; i++) {
                const panel_cfg_t *p_cfg = &temp_cfg[i];
                panel_status_t *p_status = &temp_status[i];
                if (((p_cfg->func == CURTAIN_OPEN) || (p_cfg->func == CURTAIN_CLOSE)) &&
                    (group == p_cfg->group || group == 0xFF) &&
                    temp_status[i].relay_short == true) {
                    panel_fast_exe(p_status, (0b00111010 & ~0x01) | 0x00);
                }
            }
        } break;
        case VOLUME_ADD:
            break;
        case VOLUME_SUB:
            break;
        case PLAY_PAUSE:
            break;
        case NEXT_SONG:
            break;
        case LAST_SONG:
            break;
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
    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
        temp[i].led_w_open = led_state;
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

// 执行上电参数(迎宾)
static void panel_power_status(void)
{
    const panel_cfg_t *temp_cfg = app_get_panel_cfg();

    for (uint8_t i = 0; i < KEY_NUMBER; i++) {

        if (BIT3(temp_cfg->perm) == true) { // 权限是否勾选"迎宾"
            my_panel_status[i].led_w_open = true;
            my_panel_status[i].relay_open = true;
            my_panel_status[i].key_status = true;
        }
    }
    #if defined PANEL_8KEY
    const panel_cfg_t *temp_cfg_ex = app_get_panel_cfg_ex();
    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
        if (BIT3(temp_cfg_ex[i].perm) == true) { // 权限是否勾选"迎宾"
            my_panel_status_ex[i].led_w_open = true;
            my_panel_status_ex[i].relay_open = true;
            my_panel_status_ex[i].key_status = true;
        }
    }
    #endif
}

static void panel_fast_exe(panel_status_t *temp_fast, uint8_t flag)
{
    // 0位:data->data[2];1位:key_status;2位:led_w_open,3位:led_w_short,4位:relay_open;5位:relay_short;6位:保留;7位:保留

    if (BIT1(flag) == true) {
        temp_fast->key_status = BIT0(flag);
    }
    if (BIT2(flag) == true) {
        temp_fast->led_w_open = BIT0(flag);
    }
    if (BIT3(flag) == true) {
        temp_fast->led_w_short = true; // 短亮会快速熄灭,无需持续状态,故而不受data->data[2]控制
    }
    if (BIT4(flag) == true) {
        temp_fast->relay_open = BIT0(flag);
    }
    if (BIT5(flag) == true) {
        temp_fast->relay_short = BIT0(flag);
    }
}
#endif
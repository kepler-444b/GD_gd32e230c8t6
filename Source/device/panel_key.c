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

    #define ADC_TO_VOL(adc_val) ((adc_val) * 330 / 4096) // adc值转电压

    #define ADC_VOL_NUMBER      10 // 电压值缓冲区数量

    #define MIN_VOL             329 // 无按键按下时的最小电压值
    #define MAX_VOL             330 // 无按键按下时的最大电压值

    #define SHORT_TIME_LED      100   // 短亮时间
    #define SHORT_TIME_RELAY    30000 // 继电器导通时间
    #define LONG_PRESS          300   // 长按时间

    #define FADE_TIME           500

    // 函数参数
    #define FUNC_PARAMS valid_data_t *data, const panel_cfg_t *temp_cfg, panel_status_t *temp_status
    #define FUNC_ARGS   data, temp_cfg, temp_status

    // 外层遍历
    #define PROCESS_OUTER(cfg, status, ...)               \
        do {                                              \
            for (uint8_t _i = 0; _i < KEY_NUMBER; _i++) { \
                const panel_cfg_t *p_cfg = &(cfg)[_i];    \
                panel_status_t *p_status = &(status)[_i]; \
                __VA_ARGS__                               \
            }                                             \
        } while (0)

    // 内层遍历
    #define PROCESS_INNER(cfg_ex, status_ex, ...)               \
        do {                                                    \
            for (uint8_t _j = 0; _j < KEY_NUMBER; _j++) {       \
                const panel_cfg_t *p_cfg_ex = &(cfg_ex)[_j];    \
                panel_status_t *p_status_ex = &(status_ex)[_j]; \
                __VA_ARGS__                                     \
            }                                                   \
        } while (0)

typedef struct {
    // 用于处理adc数据
    uint8_t buf_idx; // 缓冲区下标
    uint16_t vol;    // 电压值
    uint16_t vol_buf[ADC_VOL_NUMBER];
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
    uint32_t relay_open_count;  // 继电器短开计数器
    const key_vol_t vol_range;  // 按键电压范围

} panel_status_t;

typedef struct
{
    bool led_filck;           // 闪烁
    bool key_long_press;      // 长按状态
    bool enter_config;        // 进入配置状态
    uint16_t back_lum;        // 所有背光灯亮度
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
    [3] = {.vol_range = {145, 155}},
};

    #endif

static void panel_gpio_init(void);
static void panel_power_status(void);
static void panel_data_cb(valid_data_t *data);
static void panel_read_adc(TimerHandle_t xTimer);
static void panel_proce_cmd(TimerHandle_t xTimer);
static void panel_ctrl_led_all(bool led_state, bool is_ex);
static void panel_event_handler(event_type_e event, void *params);
static void process_led_flicker(common_panel_t *common_panel, bool is_ex_panel);
static void panel_fast_exe(panel_status_t *temp_fast, uint8_t flag);
static void process_exe_status(const panel_cfg_t *temp_cfg, panel_status_t *temp_status);
static void process_exe_power(const panel_cfg_t *temp_cfg, panel_status_t *temp_status);
static void process_panel_adc(panel_status_t *panel_status, common_panel_t *common_panel, adc_value_t *adc_value, bool is_ex);
static void process_cmd_check(FUNC_PARAMS);
static void panel_all_close(FUNC_PARAMS);
static void panel_all_on_off(FUNC_PARAMS);
static void panel_curtain_open(FUNC_PARAMS);
static void panel_curtain_close(FUNC_PARAMS);
static void panel_curtain_stop(FUNC_PARAMS);
static void panel_clean_room(FUNC_PARAMS);
static void panel_dnd_mode(FUNC_PARAMS);
static void panel_later_mode(FUNC_PARAMS);
static void panel_chect_out(FUNC_PARAMS);
static void panel_sos_mode(FUNC_PARAMS);
static void panel_service(FUNC_PARAMS);
static void panel_scene_mode(FUNC_PARAMS);
static void panel_light_mode(FUNC_PARAMS);
static void panel_night_light(FUNC_PARAMS);
// #endregion

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
    my_adc_value.vol = ADC_TO_VOL(app_get_adc_value()[0]);
    process_panel_adc(my_panel_status, &my_common_panel, &my_adc_value, false);
    if (my_common_panel.led_filck) {
        process_led_flicker(&my_common_panel, false);
    }
    #if defined PANEL_8KEY
    my_adc_value_ex.vol = ADC_TO_VOL(app_get_adc_value()[1]);
    process_panel_adc(my_panel_status_ex, &my_common_panel_ex, &my_adc_value_ex, true);
    if (my_common_panel_ex.led_filck) {
        process_led_flicker(&my_common_panel_ex, true);
    }
    #endif
}

static void process_panel_adc(panel_status_t *panel_status, common_panel_t *common_panel, adc_value_t *adc_value, bool is_ex)
{
    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
        panel_status_t *p_status = &panel_status[i];
        if (adc_value->vol < p_status->vol_range.min || adc_value->vol > p_status->vol_range.max) {
            if (adc_value->vol >= MIN_VOL && adc_value->vol <= MAX_VOL) {
                p_status->key_press          = false;
                common_panel->key_long_press = false;
                common_panel->key_long_count = 0;
            }
            continue;
        }
        // 填充 vol_buf
        adc_value->vol_buf[adc_value->buf_idx++] = adc_value->vol;
        if (adc_value->buf_idx < ADC_VOL_NUMBER) {
            continue;
        }
        adc_value->buf_idx = 0; // vol_buf 被填充满(下标置0)
        uint16_t new_value = app_calculate_average(adc_value->vol_buf, ADC_VOL_NUMBER);
        if (new_value < p_status->vol_range.min || new_value > p_status->vol_range.max) {
            continue; // 检查平均值是否在有效范围
        }
        if (!p_status->key_press && !common_panel->enter_config) { // 处理按键按下
            p_status->key_status ^= 1;
            const bool is_special = p_status->relay_short;
            app_send_cmd(i, (is_special ? 0x00 : p_status->key_status), PANEL_HEAD, (is_special ? SPECIAL_CMD : COMMON_CMD), is_ex);

            p_status->key_press          = true;
            common_panel->key_long_press = true;
            common_panel->key_long_count = 0;
            continue;
        }
        // 处理长按
        if (common_panel->key_long_press && ++common_panel->key_long_count >= LONG_PRESS) {
            app_send_cmd(0, 0, APPLY_CONFIG, 0x00, is_ex);
            common_panel->key_long_press = false;
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
    PROCESS_OUTER(temp_cfg, temp_status, {
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
            p_status->led_w_last = p_status->led_w_open;
            app_set_pwm_fade(p_cfg->led_y_pin, p_status->led_w_open ? 0 : 500, FADE_TIME);
        }

        if (p_status->relay_open != p_status->relay_last) { // 继电器状态更新
            for (uint8_t i = 0; i < RELAY_NUMBER; i++) {
                if (!app_gpio_equal(p_cfg->relay_pin[i], DEF)) { // 只操作勾选的继电器
                    APP_SET_GPIO(p_cfg->relay_pin[i], p_status->relay_open);
                }
            }
            p_status->relay_last = p_status->relay_open;
            // APP_PRINTF("%d %d %d %d\n", APP_GET_GPIO(PB12), APP_GET_GPIO(PB13), APP_GET_GPIO(PB14), APP_GET_GPIO(PB15));
        }
    });
}

static void panel_data_cb(valid_data_t *data)
{
    // 0:固定码,1:按键功能,2:操作指令,3:分组,4:总关/场景区域,5:校验码,6:按键权限,7:场景分组
    APP_PRINTF_BUF("[RECV]", data->data, data->length);
    const panel_cfg_t *temp_cfg = app_get_panel_cfg();
    process_cmd_check(data, temp_cfg, my_panel_status);

    #if defined PANEL_8KEY
    const panel_cfg_t *temp_cfg_ex = app_get_panel_cfg_ex();
    process_cmd_check(data, temp_cfg_ex, my_panel_status_ex);
    #endif
}

static void process_cmd_check(FUNC_PARAMS)
{
    switch (data->data[1]) {
        case ALL_CLOSE: // 总关
            panel_all_close(FUNC_ARGS);
            break;
        case ALL_ON_OFF: // 总开关
            panel_all_on_off(FUNC_ARGS);
            break;
        case CLEAN_ROOM: // 清理房间
            panel_clean_room(FUNC_ARGS);
            break;
        case DND_MODE: // 勿扰模式
            panel_dnd_mode(FUNC_ARGS);
            break;
        case LATER_MODE: // 请稍后
            panel_later_mode(FUNC_ARGS);
            break;
        case CHECK_OUT: // 退房
            panel_chect_out(FUNC_ARGS);
            break;
        case SOS_MODE: // 紧急呼叫
            panel_sos_mode(FUNC_ARGS);
            break;
        case SERVICE: // 请求服务
            panel_service(FUNC_ARGS);
            break;
        case CURTAIN_OPEN: // 窗帘开
            panel_curtain_open(FUNC_ARGS);
            break;
        case CURTAIN_CLOSE: // 窗帘关
            panel_curtain_close(FUNC_ARGS);
            break;
        case SCENE_MODE: // 场景模式
            panel_scene_mode(FUNC_ARGS);
            break;
        case LIGHT_MODE: // 灯控模式
            panel_light_mode(FUNC_ARGS);
            break;
        case NIGHT_LIGHT: // 夜灯
            panel_night_light(FUNC_ARGS);
            break;
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
        case CURTAIN_STOP: // 窗帘停
            panel_curtain_stop(FUNC_ARGS);
            break;
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
    process_exe_power(app_get_panel_cfg(), my_panel_status);
    #if defined PANEL_8KEY
    process_exe_power(app_get_panel_cfg_ex(), my_panel_status_ex);
    #endif
}

static void process_exe_power(const panel_cfg_t *temp_cfg, panel_status_t *temp_status)
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (!BIT3(p_cfg->perm)) {
            continue; // 权限是否勾选"迎宾"
        }
        switch (p_cfg->func) {
            case LIGHT_MODE:
                panel_fast_exe(p_status, 0b00010110 | 0x01);
                break;
            case SCENE_MODE: // 场景模式,不开白灯
                panel_fast_exe(p_status, 0b00010000 | 0x01);
                break;
            default:
                break;
        }
    });
}

static void panel_fast_exe(panel_status_t *temp_fast, uint8_t flag)
{
    // 0:data->data[2];1:key_status;2:led_w_open,3:led_w_short,4:relay_open;5:relay_short;6:保留;7:保留

    if (BIT1(flag)) {
        temp_fast->key_status = BIT0(flag);
    }
    if (BIT2(flag)) {
        temp_fast->led_w_open = BIT0(flag);
    }
    if (BIT3(flag)) {
        temp_fast->led_w_short = true; // 短亮会快速熄灭,无需持续状态,故而不受data->data[2]控制
    }
    if (BIT4(flag)) {
        temp_fast->relay_open = BIT0(flag);
    }
    if (BIT5(flag)) {
        temp_fast->relay_short      = BIT0(flag);
        temp_fast->relay_open_count = 0; // 继电器短开计数器提前置0
    }
}

/* ***************************************** 处理按键类型 ***************************************** */
static void panel_all_close(FUNC_PARAMS) // 总关
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (!BIT5(p_cfg->perm)) {
            continue; // 无"睡眠"权限,跳过
        }
        if (H_BIT(data->data[4]) != 0xF &&                // 非全局指令
            H_BIT(data->data[4]) != H_BIT(p_cfg->area)) { // 分区不匹配
            continue;
        }
        switch (p_cfg->func) {
            case ALL_CLOSE:
                if (data->data[3] == p_cfg->group) {
                    panel_fast_exe(p_status, 0b00011010 | (data->data[2] & 0x01));
                }
                break;
            case NIGHT_LIGHT:
            case DND_MODE:
            case ALL_ON_OFF:
                panel_fast_exe(p_status, 0b00010110 | (data->data[2] & 0x01));
                break;
            case SCENE_MODE:
            case LIGHT_MODE:

            case CLEAN_ROOM:
                panel_fast_exe(p_status, 0b00010110 | 0x00);
                break;
            case CURTAIN_CLOSE: // 勾选"总关"的窗帘关
                PROCESS_INNER(temp_cfg, temp_status, {
                    if (p_cfg_ex->func != CURTAIN_OPEN ||
                        p_cfg_ex->group != p_cfg->group) {
                        continue;
                    }
                    panel_fast_exe(p_status_ex, 0b00110000 | 0x00);
                });
                panel_fast_exe(p_status, 0b00101010 | 0x01);
                break;
            default:
                break;
        }
    });
}

static void panel_all_on_off(FUNC_PARAMS) // 总开关
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (!BIT0(p_cfg->perm)) {
            continue; // 无"总开关"权限跳过
        }
        if (H_BIT(data->data[4]) != 0xF &&                // 分区不匹配
            H_BIT(data->data[4]) != H_BIT(p_cfg->area)) { // 非全局指令
            continue;
        }
        switch (p_cfg->func) {
            case ALL_ON_OFF:
                if (data->data[3] == p_cfg->group) {
                    if (BIT4(temp_cfg[_i].perm) && BIT6(temp_cfg[_i].perm)) { //"只开" + "取反"
                        panel_fast_exe(p_status, 0b00011010 | 0x00);
                    } else if (BIT4(temp_cfg[_i].perm)) { //"只开"
                        panel_fast_exe(p_status, 0b00010110 | 0x01);
                    } else if (BIT6(temp_cfg[_i].perm)) { // "取反"
                        panel_fast_exe(p_status, 0b00010110 | (~data->data[2] & 0x01));
                    } else { // 默认
                        panel_fast_exe(p_status, 0b00010110 | (data->data[2] & 0x01));
                    }
                }
                break;
            case DND_MODE:
                panel_fast_exe(p_status, 0b00010110 | (~data->data[2] & 0x01));
                break;
            case LIGHT_MODE:
            case SCENE_MODE:
            case CLEAN_ROOM:
            case NIGHT_LIGHT:
                if (BIT7(p_cfg->perm) && !data->data[2]) { // 勾选了"不总开",则只受"总开关"的关控制
                    panel_fast_exe(p_status, 0b00010110 | 0x00);
                } else if (!BIT7(p_cfg->perm)) { // 勾选"不总开",则正常动作
                    panel_fast_exe(p_status, 0b00010110 | (data->data[2] & 0x01));
                }
                break;
            case ALL_CLOSE: // 勾选"总开关"的"总关"不执行闪烁
                panel_fast_exe(p_status, 0b00010010 | (data->data[2] & 0x01));
                break;
            case CURTAIN_CLOSE: // 勾选"总开关"的窗帘关
                if (!data->data[2]) {
                    PROCESS_INNER(temp_cfg, temp_status, {
                        if (p_cfg_ex->func != CURTAIN_OPEN ||
                            p_cfg_ex->group != p_cfg->group) {
                            continue;
                        }
                        panel_fast_exe(p_status_ex, 0b00110000 | 0x00);
                    });
                    panel_fast_exe(p_status, 0b00101010 | 0x01);
                }
                break;
            case CURTAIN_OPEN: // 勾选"总开关"的窗帘开
                if (data->data[2]) {
                    PROCESS_INNER(temp_cfg, temp_status, {
                        if (p_cfg_ex->func != CURTAIN_CLOSE ||
                            p_cfg_ex->group != p_cfg->group) {
                            continue;
                        }
                        panel_fast_exe(p_status_ex, 0b00110000 | 0x00);
                    });
                    panel_fast_exe(p_status, 0b00101010 | 0x01);
                }
                break;
            default:
                break;
        }
    });
}

static void panel_clean_room(FUNC_PARAMS) // 清理房间
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (p_cfg->func != CLEAN_ROOM ||
            (data->data[3] != p_cfg->group && data->data[3] != 0xFF)) {
            continue;
        }
        if (data->data[2]) {
            PROCESS_INNER(temp_cfg, temp_status, {
                if (p_cfg_ex->func != DND_MODE ||
                    (data->data[3] != p_cfg_ex->group && data->data[3] != 0xFF)) {
                    continue;
                }
                panel_fast_exe(p_status_ex, 0b00010110 | (~data->data[2] & 0x01));
            });
        }
        panel_fast_exe(p_status, 0b00010110 | (data->data[2] & 0x01));
    });
}

static void panel_dnd_mode(FUNC_PARAMS) // 勿扰模式
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (p_cfg->func != DND_MODE ||
            (data->data[3] != p_cfg->group && data->data[3] != 0xFF)) {
            continue;
        }
        if (data->data[2]) {
            PROCESS_INNER(temp_cfg, temp_status, {
                if (p_cfg_ex->func != CLEAN_ROOM ||
                    (data->data[3] != p_cfg_ex->group && data->data[3] != 0xFF)) {
                    continue;
                }
                panel_fast_exe(p_status_ex, 0b00010110 | (~data->data[2] & 0x01));
            });
        }
        panel_fast_exe(p_status, 0b00010110 | (data->data[2] & 0x01));
    });
}

static void panel_later_mode(FUNC_PARAMS) // 请稍后
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (p_cfg->func != LATER_MODE ||
            (data->data[3] != p_cfg->group && data->data[3] != 0xFF)) {
            continue;
        }
        panel_fast_exe(p_status, 0b00100110 | (data->data[2] & 0x01));
    });
}

static void panel_chect_out(FUNC_PARAMS) // 退房
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (p_cfg->func != CHECK_OUT ||
            (data->data[3] != p_cfg->group && data->data[3] != 0xFF)) {
            continue;
        }
        panel_fast_exe(p_status, 0b00010110 | (data->data[2] & 0x01));
    });
}

static void panel_sos_mode(FUNC_PARAMS) // 紧急呼叫
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (p_cfg->func != SOS_MODE ||
            (data->data[3] != p_cfg->group && data->data[3] != 0xFF)) {
            continue;
        }
        panel_fast_exe(p_status, 0b00010110 | (data->data[2] & 0x01));
    });
}

static void panel_service(FUNC_PARAMS) // 请求服务
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (p_cfg->func != SERVICE ||
            (data->data[3] != p_cfg->group && data->data[3] != 0xFF)) {
            continue;
        }
        panel_fast_exe(p_status, 0b00010110 | (data->data[2] & 0x01));
    });
}

static void panel_curtain_open(FUNC_PARAMS) // 窗帘开
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (p_cfg->func != CURTAIN_OPEN ||
            (data->data[3] != p_cfg->group && data->data[3] != 0xFF)) {
            continue;
        }
        PROCESS_INNER(temp_cfg, temp_status, {
            if (p_cfg_ex->func != CURTAIN_CLOSE ||
                (data->data[3] != p_cfg_ex->group && data->data[3] != 0xFF)) {
                continue;
            }
            panel_fast_exe(p_status_ex, 0b00110000 | 0x00);
        });
        panel_fast_exe(p_status, 0b00101010 | 0x01);
    });
}

static void panel_curtain_close(FUNC_PARAMS) // 窗帘关
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (p_cfg->func != CURTAIN_CLOSE ||
            (data->data[3] != p_cfg->group && data->data[3] != 0xFF)) {
            continue;
        }
        PROCESS_INNER(temp_cfg, temp_status, {
            if (p_cfg_ex->func != CURTAIN_OPEN ||
                (data->data[3] != p_cfg_ex->group && data->data[3] != 0xFF)) {
                continue;
            }
            panel_fast_exe(p_status_ex, 0b00110000 | 0x00);
        });
        panel_fast_exe(p_status, 0b00101010 | 0x01);
    });
}

static void panel_curtain_stop(FUNC_PARAMS) // 窗帘停
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (((p_cfg->func != CURTAIN_OPEN) && (p_cfg->func != CURTAIN_CLOSE)) ||
            (data->data[3] != p_cfg->group && data->data[3] != 0xFF) ||
            !p_status->relay_short) {
            continue;
        }
        panel_fast_exe(p_status, 0b00111010 | 0x00);
    });
}

static void panel_scene_mode(FUNC_PARAMS) // 场景模式
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (((L_BIT(data->data[4]) != L_BIT(p_cfg->area)) &&
             (L_BIT(data->data[4]) != 0xF))) { // 匹配场景区域
            continue;
        }
        panel_fast_exe(p_status, 0b00010110 | 0x00); // 关闭该场景区域所有的按键

        uint8_t mask = data->data[7] & p_cfg->scene_group; // 找出两个字节中同为1的位
        if (!mask) {                                       // 任意一位同为1,说明勾选了该场景分组,执行动作
            continue;
        }
        switch (p_cfg->func) {
            case CURTAIN_OPEN: // 勾选此场景的"窗帘开"
                if (data->data[2]) {
                    PROCESS_INNER(temp_cfg, temp_status, {
                        if (p_cfg_ex->func == CURTAIN_CLOSE &&
                            p_cfg_ex->group == p_cfg->group) {
                            panel_fast_exe(p_status_ex, 0b00110000 | 0x00);
                        }
                    });
                    panel_fast_exe(p_status, 0b00101010 | 0x01);
                }
                break;
            case CURTAIN_CLOSE: // 勾选此场景的"窗帘关"
                if (!data->data[2]) {
                    PROCESS_INNER(temp_cfg, temp_status, {
                        if (p_cfg_ex->func == CURTAIN_OPEN &&
                            (data->data[3] == p_cfg_ex->group || data->data[3] == 0xFF)) {
                            panel_fast_exe(p_status_ex, 0b00110000 | 0x00);
                        }
                    });
                    panel_fast_exe(p_status, 0b00101010 | 0x01);
                }
                break;
            case LIGHT_MODE:
            case SCENE_MODE:
            case ALL_ON_OFF:
                panel_fast_exe(p_status, 0b00010110 | (data->data[2] & 0x01));
                break;
            default:
                break;
        }
    });
}

static void panel_light_mode(FUNC_PARAMS) // 灯控模式
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (p_cfg->func != LIGHT_MODE ||
            (data->data[3] != p_cfg->group && data->data[3] != 0xFF)) {
            continue;
        }
        panel_fast_exe(p_status, 0b00010110 | (data->data[2] & 0x01));
    });
}

static void panel_night_light(FUNC_PARAMS) // 夜灯
{
    PROCESS_OUTER(temp_cfg, temp_status, {
        if (p_cfg->func != NIGHT_LIGHT ||
            (data->data[3] != p_cfg->group && data->data[3] != 0xFF)) {
            continue;
        }
        panel_fast_exe(p_status, 0b00010110 | (data->data[2] & 0x01));
    });
}
#endif
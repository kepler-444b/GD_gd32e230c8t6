#include "gd32e23x.h"
#include "quick_box.h"
#include "device_manager.h"
#include "FreeRTOS.h"
#include "timers.h"
#include "../gpio/gpio.h"
#include "../base/debug.h"
#include "../usart/usart.h"
#include "../protocol/protocol.h"
#include "../pwm/pwm.h"
#include "../eventbus/eventbus.h"
#include "../config/config.h"
#include "../base/base.h"
#include "../zero/zero.h"
#include "../device/pcb_device.h"

#if defined QUICK_BOX_WZR
    #define SYSTEM_CLOCK_FREQ  72000000 // 系统时钟频率(72MHz)
    #define TIMER_PERIOD       999      // 定时器周期(1ms中断)

    #define TO_500(x)          ((x) <= 0 ? 0 : ((x) >= 100 ? 500 : ((x) * 500) / 100))
    #define SCALE_5_TO_5000(x) ((x) <= 0 ? 0 : ((x) >= 5 ? 5000 : ((x) * 5000) / 5))

    #define FUNC_PARAMS        valid_data_t *data, const quick_ctg_t *temp_cfg
    #define FUNC_ARGS          data, temp_cfg

    // 遍历
    #define PROCESS(cfg, ...)                             \
        do {                                              \
            for (uint8_t _i = 0; _i < LED_NUMBER; _i++) { \
                const quick_ctg_t *p_cfg = &(cfg)[_i];    \
                __VA_ARGS__                               \
            }                                             \
        } while (0)

    #define FADE_TIME  1000
    #define LONG_PRESS 3000

typedef struct {
    bool key_status;          // 按键状态
    bool led_filck;           // 闪烁
    bool enter_config;        // 进入配置状态
    uint16_t key_long_count;  // 长按计数
    uint32_t led_filck_count; // 闪烁计数

} common_quick_t;
static common_quick_t my_common_quick = {0};

// 函数声明
static void quick_box_data_cb(valid_data_t *data);
static void quick_event_handler(event_type_e event, void *params);
static void quick_box_timer(TimerHandle_t xTimer);
static void quick_ctrl_led_all(bool status);
static void quick_fast_exe(uint8_t len_num, uint16_t lum);
static void process_led_flicker(void);

static void quick_scene_exe(FUNC_PARAMS);
static void quick_all_close(FUNC_PARAMS);
static void quick_all_on_off(FUNC_PARAMS);
static void quick_scene_mode(FUNC_PARAMS);
static void quick_light_mode(FUNC_PARAMS);

void quick_box_init(void)
{
    APP_PRINTF("quick_box_init\n");
    quick_box_gpio_init();
    app_eventbus_subscribe(quick_event_handler); // 订阅事件总线

    // 初始化一个静态定时器,用于检测值
    static StaticTimer_t QuickStaticBuffer;
    static TimerHandle_t QuickTimerHandle = NULL;

    QuickTimerHandle = xTimerCreateStatic(
        "QuickTimer",
        pdMS_TO_TICKS(1),
        pdTRUE,
        NULL,
        quick_box_timer,
        &QuickStaticBuffer);

    if (xTimerStart(QuickTimerHandle, 0) != true) { // 启动定时器
        APP_ERROR("QuickTimerHandle error");
    }
    app_pwm_init();
    app_pwm_add_pin(PB7);
    app_pwm_add_pin(PB6);
    app_pwm_add_pin(PB5);

    app_zero_init(EXTI_11); // 过零检测外部中断配置(PB11)
}

static void quick_box_data_cb(valid_data_t *data)
{
    APP_PRINTF_BUF("[RECV]", data->data, data->length);
    const quick_ctg_t *temp_cfg = app_get_quick_cfg();
    switch (data->data[1]) {
        case ALL_CLOSE: // 总关
            quick_all_close(FUNC_ARGS);
            break;
        case ALL_ON_OFF: // 总开关
            quick_all_on_off(FUNC_ARGS);
            break;
        case SCENE_MODE: // 场景模式
            quick_scene_mode(FUNC_ARGS);
            break;
        case LIGHT_MODE: // 灯控模式
            quick_light_mode(FUNC_ARGS);
        default:
            return;
    }
}

static void quick_event_handler(event_type_e event, void *params)
{
    switch (event) {
        case EVENT_ENTER_CONFIG: { // 进入配置模式
            quick_ctrl_led_all(true);
            my_common_quick.enter_config = true;
        } break;
        case EVENT_EXIT_CONFIG: { // 退出配置模式
            quick_ctrl_led_all(false);
            my_common_quick.enter_config = false;
            NVIC_SystemReset(); // 退出"配置模式"后触发软件复位
        } break;
        case EVENT_SAVE_SUCCESS:
            my_common_quick.led_filck = true;
            break;
        case EVENT_RECEIVE_CMD: {
            valid_data_t *valid_data = (valid_data_t *)params;
            quick_box_data_cb(valid_data);
        } break;
        default:
            return;
    }
}

static void quick_box_timer(TimerHandle_t xTimer)
{
    my_common_quick.key_status = APP_GET_GPIO(PA0);

    if (!my_common_quick.key_status) { // 按键按下
        my_common_quick.key_long_count++;
        if (my_common_quick.key_long_count >= 5000) { // 触发长按
            app_send_cmd(0, 0, APPLY_CONFIG, COMMON_CMD, false);
            my_common_quick.key_long_count = 0;
            APP_PRINTF("long_press\n");
        }
    } else if (my_common_quick.key_status && my_common_quick.key_long_count) {
        my_common_quick.key_long_count = 0;
    }
    if (my_common_quick.led_filck) { // 开始闪烁
        process_led_flicker();
    }
}

static void quick_ctrl_led_all(bool status)
{
    for (uint8_t i = 0; i < 3; i++) {
        app_set_pwm_duty(app_get_quick_cfg()[i].led_pin, status ? 500 : 0);
    }
}

static void process_led_flicker(void) // led 闪烁
{
    my_common_quick.led_filck_count++;
    if (my_common_quick.led_filck_count <= 500) {
        quick_ctrl_led_all(false);
    } else {
        quick_ctrl_led_all(true);
        my_common_quick.led_filck_count = 0;     // 重置计数器
        my_common_quick.led_filck       = false; // 停止闪烁
    }
}

static void quick_fast_exe(uint8_t len_num, uint16_t lum)
{
    const quick_ctg_t *temp_cfg = app_get_quick_cfg();
    if (len_num < 3) {
        app_set_pwm_fade(temp_cfg[len_num].led_pin, lum, FADE_TIME);
    } else {
        zero_set_gpio(temp_cfg[len_num].led_pin, lum);
    }
}

/* ***************************************** 处理按键类型 ***************************************** */
static void quick_all_close(FUNC_PARAMS) // 总关
{
    PROCESS(temp_cfg, {
        if (!BIT5(p_cfg->perm)) {
            continue;
        }
        if (H_BIT(data->data[4]) != H_BIT(p_cfg->area) &&
            H_BIT(data->data[4]) != 0xF) {
            continue;
        }
        quick_fast_exe(_i, 0);
    });
}

static void quick_all_on_off(FUNC_PARAMS) // 总开关
{
    PROCESS(temp_cfg, {
        if (!BIT5(p_cfg->perm)) {
            continue;
        }
        if (H_BIT(data->data[4]) != 0xF &&
            H_BIT(data->data[4]) != H_BIT(p_cfg->area)) { // 检查分区匹配
            continue;
        }
        quick_fast_exe(_i, data->data[2] ? TO_500(p_cfg->lum) : 0);
    });
}

static void quick_scene_mode(FUNC_PARAMS) // 场景模式
{
    PROCESS(temp_cfg, {
        if (L_BIT(data->data[4]) != 0xF &&
            L_BIT(data->data[4]) != L_BIT(p_cfg->area)) {
            continue; // 与本场景的分区不同且场景分区不为0xF,跳过
        }
        uint8_t mask = data->data[7] & p_cfg->scene_group;
        if (!mask) { // 若未勾选该场景分组,跳过
            continue;
        }
        uint8_t scene_index = __builtin_ctz(mask); // 获取场景索引
        if (_i < 3) {
            app_set_pwm_fade(p_cfg->led_pin, data->data[2] ? TO_500(p_cfg->scene_lun[scene_index]) : 0, FADE_TIME);
        } else {
            zero_set_gpio(p_cfg->led_pin, data->data[2] ? p_cfg->scene_lun[scene_index] : 0);
        }
    });
}

static void quick_light_mode(FUNC_PARAMS) // 灯控模式
{
    PROCESS(temp_cfg, {
        if (data->data[3] != p_cfg->group && data->data[3] != 0xFF) {
            continue;
        }
        if (_i < 3) {
            app_set_pwm_fade(p_cfg->led_pin, data->data[2] ? TO_500(p_cfg->lum) : 0, FADE_TIME);
        } else {
            zero_set_gpio(p_cfg->led_pin, (data->data[2] ? p_cfg->lum : 0));
        }
    });
}
#endif
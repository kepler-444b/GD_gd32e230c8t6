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

#if defined QUICK_BOX
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
    bool key_status;   // 按键状态
    bool led_filck;    // 闪烁
    bool enter_config; // 进入配置状态
    // bool relay_open[3];       // 继电器状态标志位
    // bool relay_last[3];       // 继电器上次状态
    uint16_t key_long_count;  // 长按计数
    uint32_t led_filck_count; // 闪烁计数

} common_quick_t;
static common_quick_t my_common_quick = {0};

// 函数声明
void quick_box_gpio_init(void);
static void quick_zero_init(void);
static void quick_box_data_cb(valid_data_t *data);
static void quick_event_handler(event_type_e event, void *params);
static void quick_box_timer(TimerHandle_t xTimer);
static void quick_ctrl_led_all(bool status);
static void quick_fast_exe(uint8_t len_num, uint16_t lum);
static void process_led_flicker(void);

static void quick_scene_exe(FUNC_PARAMS);
static void panel_all_close(FUNC_PARAMS);
static void panel_all_on_off(FUNC_PARAMS);
static void panel_scene_mode(FUNC_PARAMS);
static void panel_light_mode(FUNC_PARAMS);

void quick_box_init(void)
{
    APP_PRINTF("quick_box_init\n");
    quick_box_gpio_init();
    app_eventbus_subscribe(quick_event_handler); // 订阅事件总线

    quick_zero_init(); // 初始化零点检测

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

    if (xTimerStart(QuickTimerHandle, 0) != pdPASS) { // 启动定时器
        APP_ERROR("QuickTimerHandle error");
    }
    app_pwm_init();
    app_pwm_add_pin(PB7);
    app_pwm_add_pin(PB6);
    app_pwm_add_pin(PB5);

    app_zero_init(EXTI_11);
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
}

static void quick_zero_init(void)
{
    #if 0
    // 过零检测外部中断配置(PB11)
    rcu_periph_clock_enable(RCU_CFGCMP);                           // EXIT 线路与 GPIO 相连接
    syscfg_exti_line_config(EXTI_SOURCE_GPIOB, EXTI_SOURCE_PIN11); // GPIO 配置为外部中断
    exti_init(EXTI_11, EXTI_INTERRUPT, EXTI_TRIG_RISING);          // 上升沿触发
    nvic_irq_enable(EXTI4_15_IRQn, 3);

    // 定时器配置,用于精准延时(TIMER5)
    rcu_periph_clock_enable(RCU_TIMER5);
    timer_deinit(TIMER5);
    timer_parameter_struct timer_cfg;
    timer_cfg.prescaler        = (SYSTEM_CLOCK_FREQ / 1000000) - 1;
    timer_cfg.alignedmode      = TIMER_COUNTER_EDGE;
    timer_cfg.counterdirection = TIMER_COUNTER_UP;
    timer_cfg.period           = TIMER_PERIOD;
    timer_cfg.clockdivision    = TIMER_CKDIV_DIV1;
    timer_init(TIMER5, &timer_cfg);
    timer_interrupt_flag_clear(TIMER5, TIMER_INT_FLAG_UP);
    timer_interrupt_enable(TIMER5, TIMER_INT_UP);
    timer_enable(TIMER5);
    nvic_irq_enable(TIMER5_IRQn, 3);
    #endif
}

    #if 0 
void EXTI4_15_IRQHandler(void)
{
    if (exti_interrupt_flag_get(EXTI_11) == RESET) {
        return;
    }
    exti_interrupt_flag_clear(EXTI_11);
    for (uint8_t i = 0; i < 3; i++) {
        if (my_common_quick.relay_last[i] == my_common_quick.relay_open[i]) {
            continue;
        }
        APP_SET_GPIO(app_get_quick_cfg()[i + 3].led_pin, my_common_quick.relay_open[i]);
        my_common_quick.relay_last[i] = my_common_quick.relay_open[i];
    }
}

void TIMER5_IRQHandler(void)
{
    if (timer_interrupt_flag_get(TIMER5, TIMER_INT_FLAG_UP)) {
        timer_interrupt_flag_clear(TIMER5, TIMER_INT_FLAG_UP);
        APP_PRINTF("TIMER5_IRQHandler\n");
    }
}
    #endif

static void quick_box_data_cb(valid_data_t *data)
{
    APP_PRINTF_BUF("[RECV]", data->data, data->length);
    const quick_ctg_t *temp_cfg = app_get_quick_cfg();
    switch (data->data[1]) {
        case ALL_CLOSE: // 总关
            panel_all_close(FUNC_ARGS);
            break;
        case ALL_ON_OFF: // 总开关
            panel_all_on_off(FUNC_ARGS);
            break;
        case SCENE_MODE: // 场景模式
            panel_scene_mode(FUNC_ARGS);
            break;
        case LIGHT_MODE: // 灯控模式
            panel_light_mode(FUNC_ARGS);
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
        // my_common_quick.relay_open[len_num - 3] = lum;
        zero_set_gpio(temp_cfg[len_num - 3].led_pin, lum);
    }
}
static void panel_all_close(FUNC_PARAMS) // 总关
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

static void panel_all_on_off(FUNC_PARAMS) // 总开关
{
    PROCESS(temp_cfg, {
        if (!BIT5(p_cfg->perm)) {
            continue;
        }
        if (H_BIT(data->data[4]) != H_BIT(p_cfg->area) && H_BIT(data->data[4]) != 0xF) { // 检查分区匹配
            continue;
        }
        quick_fast_exe(_i, data->data[2] ? TO_500(p_cfg->lum) : 0);
    });
}

static void panel_scene_mode(FUNC_PARAMS) // 场景模式
{
    PROCESS(temp_cfg, {
        for (uint8_t i = 0; i < 8; i++) {
            // i 即 场景分组
            uint8_t mask = data->data[7] & (1 << i);
            if (!mask) { // 未勾选该场景分组,跳过
                continue;
            }
            if (p_cfg->scene_lun[i] == 0xFF) {
                continue; // 该场景分组的本路调光为0xFF,跳过
            }
            if (L_BIT(data->data[4]) != 0xF && L_BIT(data->data[4]) != L_BIT(p_cfg->area)) {
                continue; // 与本场景的分区不同且场景分区不为0xF,跳过
            }
            if (_i < 3) {
                app_set_pwm_fade(p_cfg->led_pin, data->data[2] ? TO_500(p_cfg->scene_lun[i]) : 0, FADE_TIME);
            } else {
                // my_common_quick.relay_open[_i - 3] = data->data[2] ? p_cfg->scene_lun[i] : 0;
                zero_set_gpio(p_cfg->led_pin, (data->data[2] ? p_cfg->scene_lun[i] : 0));
            }
        }
    });
}
static void panel_light_mode(FUNC_PARAMS) // 灯控模式
{
    PROCESS(temp_cfg, {
        if (p_cfg->func != LIGHT_MODE) {
            continue;
        }
        if (data->data[3] != p_cfg->group && data->data[3] != 0xFF) {
            continue;
        }
        if (_i < 3) {
            app_set_pwm_fade(p_cfg->led_pin, data->data[2] ? TO_500(p_cfg->lum) : 0, FADE_TIME);
        } else {
            // my_common_quick.relay_open[_i - 3] = data->data[2] ? p_cfg->lum : 0;
            zero_set_gpio(p_cfg->led_pin, (data->data[2] ? p_cfg->lum : 0));
        }
    });
}
#endif
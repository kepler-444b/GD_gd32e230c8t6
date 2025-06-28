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

#if defined QUICK_BOX

    #define SCALE_100_TO_500(x) ((x) <= 0 ? 0 : ((x) >= 100 ? 500 : ((x) * 500) / 100))
    #define SCALE_5_TO_5000(x)  ((x) <= 0 ? 0 : ((x) >= 5 ? 5000 : ((x) * 5000) / 5))

    #define FADE_TIME           1000
typedef struct {
    bool key_status;          // 按键状态
    bool led_filck;           // 闪烁
    bool enter_config;        // 进入配置状态
    uint16_t key_long_count;  // 长按计数
    uint32_t led_filck_count; // 闪烁计数
} common_quick_t;
static common_quick_t my_common_quick = {0};

typedef struct
{
    bool tigger_relay;
    bool relay_status;
} tigger_relay_t;
static tigger_relay_t my_tigger_relay[3] = {0};

static gpio_pin_t relay_pins[3] = {0};

// 函数声明
void quick_box_gpio_init(void);
void quick_zero_init(void);
void quick_box_data_cb(valid_data_t *data);
void quick_event_handler(event_type_e event, void *params);
void quick_box_timer(TimerHandle_t xTimer);
void quick_ctrl_led_all(bool status);
void quick_tigger_relay(void);
static void quick_fast_exe(uint8_t len_num, uint16_t lum);

static void process_led_flicker(void);

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

void quick_zero_init(void)
{
    // (零点检测对实时性要求高,摒弃轮询方式,采用外部中断触发)
    rcu_periph_clock_enable(RCU_CFGCMP);                           // EXIT 线路与 GPIO 相连接
    syscfg_exti_line_config(EXTI_SOURCE_GPIOB, EXTI_SOURCE_PIN11); // GPIO 配置为外部中断
    exti_init(EXTI_11, EXTI_INTERRUPT, EXTI_TRIG_RISING);          // 上升沿触发
    nvic_irq_enable(EXTI4_15_IRQn, 3);
}

void EXTI4_15_IRQHandler(void)
{
    if (exti_interrupt_flag_get(EXTI_11) != RESET) {
        exti_interrupt_flag_clear(EXTI_11);
        quick_tigger_relay();
    }
}

// 触发继电器
void quick_tigger_relay(void)
{
    for (uint8_t i = 0; i < 3; i++) {
        if (my_tigger_relay[i].relay_status == true &&
            my_tigger_relay[i].tigger_relay == true) {

            APP_SET_GPIO(relay_pins[i], true);
            my_tigger_relay[i].tigger_relay = false;
        } else if (my_tigger_relay[i].relay_status == false &&
                   my_tigger_relay[i].tigger_relay == true) {

            APP_SET_GPIO(relay_pins[i], false);
            my_tigger_relay[i].tigger_relay = false;
        }
    }
}

void quick_box_data_cb(valid_data_t *data)
{
    APP_PRINTF_BUF("[RECV]", data->data, data->length);
    uint8_t cmd   = data->data[1];
    uint8_t sw    = data->data[2];
    uint8_t group = data->data[3];
    uint8_t area  = data->data[4];

    const quick_ctg_t *temp_cfg = app_get_quick_cfg();
    switch (cmd) {
        case ALL_CLOSE: {
            for (uint8_t i = 0; i < LED_NUMBER; i++) {
                const quick_ctg_t *p_cfg = &temp_cfg[i];
                if ((BIT5(p_cfg->perm) == true) && // "睡眠"被勾选,"总开关分区"相同 或 0xF
                    ((H_BIT(area) == H_BIT(p_cfg->area)) || (H_BIT(area) == 0xF))) {
                    quick_fast_exe(i, 0);
                }
            }
        } break;
        case ALL_ON_OFF: {
            for (uint8_t i = 0; i < LED_NUMBER; i++) {
                const quick_ctg_t *p_cfg = &temp_cfg[i];
                if ((BIT5(p_cfg->perm) == true) && // "总开关"被勾选,"总开关分区"相同 或 0xF
                    ((H_BIT(area) == H_BIT(p_cfg->area)) || (H_BIT(area) == 0xF))) {
                    quick_fast_exe(i, sw ? p_cfg->lum : 0);
                }
            }
        } break;
        case SCENE_MODE: {
            for (uint8_t i = 0; i < LED_NUMBER; i++) {
                const quick_ctg_t *p_cfg = &temp_cfg[i];
                if ((p_cfg->scene_group != 0x00) && // 屏蔽掉没有勾选任何场景分组的按键
                    ((L_BIT(area) == L_BIT(p_cfg->area)) || (L_BIT(area) == 0xF))) {
                    uint8_t mask = data->data[7] & p_cfg->scene_group; // 找出两个字节中同为1的位
                    if (mask != 0) {                                   // 任意一位同为1,说明勾选了该场景分组,执行动作
                    }
                }
            }
            break;
            default:
                return;
        }
    }
}

void quick_event_handler(event_type_e event, void *params)
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

void quick_box_timer(TimerHandle_t xTimer)
{
    my_common_quick.key_status = APP_GET_GPIO(PA0);

    if (my_common_quick.key_status == false) { // 按键按下
        my_common_quick.key_long_count++;
        if (my_common_quick.key_long_count >= 5000) { // 触发长按
            app_send_cmd(0, 0, APPLY_CONFIG, COMMON_CMD, false);
            my_common_quick.key_long_count = 0;
            APP_PRINTF("long_press\n");
        }
    } else if (my_common_quick.key_status == true && my_common_quick.key_long_count != 0) {
        my_common_quick.key_long_count = 0;
    }

    if (my_common_quick.led_filck == true) { // 开始闪烁
        process_led_flicker();
    }
}

void quick_ctrl_led_all(bool status)
{
    const quick_ctg_t *led_cfg = app_get_quick_cfg();
    for (uint8_t i = 0; i < LED_NUMBER; i++) {
        const uint16_t pwm_duty = status ? 500 : 0;
        if (i < 3) {
            app_set_pwm_duty(led_cfg[i].led_pin, pwm_duty);
        } else {
            APP_SET_GPIO(led_cfg[i].led_pin, status);
        }
    }
}

static void process_led_flicker(void)
{
    my_common_quick.led_filck_count++;
    if (my_common_quick.led_filck_count <= 500) {
        quick_ctrl_led_all(false);
    } else {
        quick_ctrl_led_all(true);
        my_common_quick.led_filck_count = 0; // 重置计数器

        my_common_quick.led_filck = false; // 停止闪烁
    }
}
static void quick_fast_exe(uint8_t len_num, uint16_t lum)
{
    const quick_ctg_t *temp_cfg = app_get_quick_cfg();
    if (len_num < 3) {
        app_set_pwm_fade(temp_cfg[len_num].led_pin, lum, FADE_TIME);
    } else {
        APP_SET_GPIO(temp_cfg[len_num].led_pin, lum != 0);
    }
}
static void quick_scene_exe(void)
{
}
#endif
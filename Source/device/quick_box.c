#include "gd32e23x.h"
#include "quick_box.h"
#include "device_manager.h"
#include "FreeRTOS.h"
#include "timers.h"
#include "../gpio/gpio.h"
#include "../base/debug.h"
#include "../uart/uart.h"
#include "../protocol/protocol.h"
#include "../pwm/pwm.h"
#include "../eventbus/eventbus.h"
#include "../config/config.h"

#if defined QUICK_BOX

#define SCALE_100_TO_500(x) ((x) <= 0 ? 0 : ((x) >= 100 ? 500 : ((x) * 500) / 100))
#define SCALE_5_TO_5000(x)  ((x) <= 0 ? 0 : ((x) >= 5 ? 5000 : ((x) * 5000) / 5))

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
    bool led_open;
    bool led_last;
} quick_status_t;
static quick_status_t my_quick_status[LED_NUMBER_COUNT] = {0};

void quick_box_gpio_init(void);
void quick_box_data_cb(valid_data_t *data);
void quick_event_handler(event_type_e event, void *params);
void quick_box_timer(TimerHandle_t xTimer);
void quick_ctrl_led_all(bool status);

void quick_box_init(void)
{
    APP_PRINTF("quick_box_init\n");
    quick_box_gpio_init();

    // 上电后,先关闭所有灯
    APP_SET_GPIO(PB5, false);
    APP_SET_GPIO(PB6, false);
    APP_SET_GPIO(PB7, false);

    // 订阅事件总线
    app_eventbus_subscribe(quick_event_handler);

    // 初始化一个静态定时器,用于检测值
    static StaticTimer_t CheckZeroStaticBuffer;
    static TimerHandle_t CheckZeroTimerHandle = NULL;

    CheckZeroTimerHandle = xTimerCreateStatic(
        "QuickTimer",          // 定时器名称(调试用)
        pdMS_TO_TICKS(1),      // 定时周期(1ms)
        pdTRUE,                // 自动重载(TRUE=周期性，FALSE=单次)
        NULL,                  // 定时器ID(可用于传递参数)
        quick_box_timer,       // 回调函数
        &CheckZeroStaticBuffer // 静态内存缓冲区
    );
    // 启动定时器(0表示不阻塞)
    if (xTimerStart(CheckZeroTimerHandle, 0) != pdPASS) {
        APP_ERROR("CheckZeroTimerHandle error");
    }

    gpio_pin_typedef_t pins[3] = {PB7, PB6, PB5};
    app_pwm_init(pins, 3);
    // app_set_pwm_fade(0, 10, 1000);
    // app_set_pwm_fade(1, 10, 1000);
    // app_set_pwm_fade(2, 10, 1000);
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

void quick_box_data_cb(valid_data_t *data)
{
    APP_PRINTF_BUF("quick_recv", data->data, data->length);
    const quick_ctg_t *temp_cfg = app_get_quick_cfg();

    for (uint8_t i = 0; i < LED_NUMBER_COUNT; i++) {
        if (temp_cfg->key_packet[i].key_func == data->data[1] &&
            temp_cfg->key_packet[i].key_group == data->data[3] &&
            temp_cfg->key_packet[i].key_area == data->data[6] &&
            temp_cfg->key_packet[i].key_scene == data->data[7] &&
            temp_cfg->key_packet[i].key_perm == data->data[4]) {
            if (temp_cfg->key_packet[i].key_area & 0x10) { // 如果勾选了只开
                my_quick_status[i].led_open = true;
            } else {
                my_quick_status[i].led_open = data->data[2];
            }
            APP_PRINTF("%d\n", i);
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
        } break;
        case EVENT_SAVE_SUCCESS:
            my_common_quick.led_filck = true;
            app_load_config();
            break;
        case EVENT_RECEIVE_CMD: {
            valid_data_t *valid_data = (valid_data_t *)params;
            quick_box_data_cb(valid_data);
        }
        default:
            return;
    }
}

void quick_box_timer(TimerHandle_t xTimer)
{
    const quick_ctg_t *temp_cfg = app_get_quick_cfg();
    // FlagStatus status = APP_GET_GPIO(PB11);
    // APP_PRINTF("status:%d\n", status);
    my_common_quick.key_status = APP_GET_GPIO(PA0);

    if (my_common_quick.key_status == false) { // 按键按下
        my_common_quick.key_long_count++;
        if (my_common_quick.key_long_count >= 5000) { // 触发长按
            app_send_cmd(0, 0, 0xF8, 0x00);
            APP_PRINTF("long_press\n");
            my_common_quick.key_long_count = 0;
        }
    } else {
        my_common_quick.key_long_count = 0;
    }

    if (my_common_quick.led_filck == true) { // 开始闪烁
        my_common_quick.led_filck_count++;
        if (my_common_quick.led_filck_count <= 500) {
            quick_ctrl_led_all(true);
        } else if (my_common_quick.led_filck_count <= 1000) {
            quick_ctrl_led_all(false);
        } else {
            my_common_quick.led_filck_count = 0;     // 重置计数器
            my_common_quick.led_filck       = false; // 停止闪烁
            quick_ctrl_led_all(true);
        }
    }

    for (uint8_t i = 0; i < LED_NUMBER_COUNT; i++) { // led 轮询
        if (my_quick_status[i].led_open != my_quick_status[i].led_last) {
            if (my_quick_status[i].led_open) {
                APP_PRINTF("timer:%d\n", SCALE_5_TO_5000(temp_cfg->speed));
                app_set_pwm_fade(0, SCALE_100_TO_500(temp_cfg->key_packet[i].target[0]), SCALE_5_TO_5000(temp_cfg->speed));
                app_set_pwm_fade(1, SCALE_100_TO_500(temp_cfg->key_packet[i].target[1]), SCALE_5_TO_5000(temp_cfg->speed));
                app_set_pwm_fade(2, SCALE_100_TO_500(temp_cfg->key_packet[i].target[2]), SCALE_5_TO_5000(temp_cfg->speed));
                if (temp_cfg->key_packet->target[3] <= 100) {
                    APP_SET_GPIO(PB7, temp_cfg->key_packet->target[3]);
                }
                if (temp_cfg->key_packet->target[4] <= 100) {
                    APP_SET_GPIO(PB6, temp_cfg->key_packet->target[4]);
                }
                if (temp_cfg->key_packet->target[5] <= 100) {
                    APP_SET_GPIO(PB5, temp_cfg->key_packet->target[5]);
                }

            } else {
                app_set_pwm_fade(0, 0, SCALE_5_TO_5000(temp_cfg->speed));
                app_set_pwm_fade(1, 0, SCALE_5_TO_5000(temp_cfg->speed));
                app_set_pwm_fade(2, 0, SCALE_5_TO_5000(temp_cfg->speed));
                APP_SET_GPIO(PB7, false);
                APP_SET_GPIO(PB6, false);
                APP_SET_GPIO(PB5, false);
            }
            if (temp_cfg->key_packet[i].key_area & 0x10) { // 如果勾选了"只开"
                my_quick_status[i].led_open = false;
                my_quick_status[i].led_last = false;
            } else {
                my_quick_status[i].led_last = my_quick_status[i].led_open;
            }
        }
    }
}

void quick_ctrl_led_all(bool status)
{
    if (status == true) {
        APP_SET_GPIO(PB7, true);
        APP_SET_GPIO(PB6, true);
        APP_SET_GPIO(PB5, true);
        app_set_pwm_duty(0, 500);
        app_set_pwm_duty(1, 500);
        app_set_pwm_duty(2, 500);
    } else {
        APP_SET_GPIO(PB7, false);
        APP_SET_GPIO(PB6, false);
        APP_SET_GPIO(PB5, false);
        app_set_pwm_duty(0, 0);
        app_set_pwm_duty(1, 0);
        app_set_pwm_duty(2, 0);
    }
}
#endif
#include "gd32e23x.h"
#include "quick_box.h"
#include "device_manager.h"
#include "../gpio/gpio.h"
#include "../base/debug.h"
#include "../uart/uart.h"
#include "../protocol/protocol.h"
#include "../pwm/pwm.h"
#include "../eventbus/eventbus.h"

#if defined QUICK_BOX

typedef struct {
    bool key_status;
    uint16_t lum;
    bool mode;
} quick_box_t;
static quick_box_t my_quick_box = {0};

void quick_box_gpio_init(void);
void quick_box_data_cb(valid_data_t *data);
void quick_event_handler(event_type_e event, void *params);

void quick_box_init(void)
{
    APP_PRINTF("quick_box_init11111111111111111111111111111111111");
    quick_box_gpio_init();

    app_pwm_init(PB7, PB6, PB5, DEFAULT); //  初始化PWM
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

    // app_ctrl_gpio(PB5, false);
    // app_ctrl_gpio(PB6, false);
    // app_ctrl_gpio(PB7, false);

    app_set_pwm_fade(0, 500, 1000);
    app_set_pwm_fade(1, 500, 1000);
    app_set_pwm_fade(2, 500, 1000);

    // 订阅事件总线
    app_eventbus_subscribe(quick_event_handler);
}

void quick_box_data_cb(valid_data_t *data)
{
    APP_PRINTF_BUF("data:", data->data, data->length);
    if (data->data[1] == 0x0d) {
        switch (data->data[7]) // 按键分组
        {
            // case 0x00: // 总关
            // {
            //     app_set_pwm_fade(0, 0, 1000);
            //     app_set_pwm_fade(1, 0, 1000);
            //     app_set_pwm_fade(2, 0, 1000);
            // } break;
            case 0x01: // 明亮
            {
                app_set_pwm_fade(0, 500, 1000);
                app_set_pwm_fade(1, 500, 1000);
                app_set_pwm_fade(2, 500, 1000);
            } break;
            case 0x08: // 温馨
            {
                app_set_pwm_fade(0, 100, 1000);
                app_set_pwm_fade(1, 100, 1000);
                app_set_pwm_fade(2, 100, 1000);
            } break;
        }
    } else if (data->data[1] == 0x00) {
        app_set_pwm_fade(0, 0, 1000);
        app_set_pwm_fade(1, 0, 1000);
        app_set_pwm_fade(2, 0, 1000);
    }
}

void quick_event_handler(event_type_e event, void *params)
{
    valid_data_t *valid_data = (valid_data_t *)params;
    APP_PRINTF_BUF("quick:", valid_data->data, valid_data->length);
    if (valid_data->data[2] == true) {
        app_set_pwm_fade(0, 500, 1000);
        app_set_pwm_fade(1, 500, 1000);
        app_set_pwm_fade(2, 500, 1000);
    } else if (valid_data->data[2] == false) {
        app_set_pwm_fade(0, 0, 1000);
        app_set_pwm_fade(1, 0, 1000);
        app_set_pwm_fade(2, 0, 1000);
    }
}
#endif
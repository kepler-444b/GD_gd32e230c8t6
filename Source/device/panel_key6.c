#include "panel_key6.h"
#include <float.h>
#include "gd32e23x.h"
#include "systick.h"
#include "../base/debug.h"
#include "../base/base.h"
#include "../switch/led.h"
#include "../adc/adc.h"
#include "../timer/timer.h"
#include "../gpio/gpio.h"
#include "../uart/uart.h"


#define KEY_NUMBER_COUNT (sizeof(my_key_config) / sizeof(my_key_config[0]))   // 按键数量
#define ADC_VALUE_COUNT 10    // 电压值缓冲区数量 
static float adc_value_buffer[ADC_VALUE_COUNT];
static uint8_t adc_index = 0;
static uint8_t buffer_full  = 0; // 标记缓冲区是否已满




// 此结构体用于配置每个按键对应的电压范围
typedef struct 
{
    float min;
    float max;
}key_config;
// static const key_config my_key_config[] = {{1.45f, 1.55f,}, // 按键1
//                                            {1.95f, 2.10f,}, // 按键2
//                                            {0.00f, 0.15f,}, // 按键3
//                                            {0.85f, 1.10f,}, // 按键4
//                                            {2.30f, 2.55f,}, // 按键5
//                                            {0.25f, 0.45f,}, // 按键6
//                                            };
static const key_config my_key_config[] = { {0.00f, 0.15f,}, // 按键1
                                            {0.25f, 0.45f,}, // 按键2
                                            {0.85f, 1.10f,}, // 按键3
                                            {1.45f, 1.55f,}, // 按键4
                                            {1.95f, 2.10f,}, // 按键5
                                            {2.30f, 2.55f,}, // 按键6
                                            };                         
typedef struct
{
    bool key_status;
    bool led_w_status;
}panel_stats;
static panel_stats my_panel_status[KEY_NUMBER_COUNT] = {0};


// 函数声明
void panel_gpio_init(void);
void panel_ctrl_led(uint8_t led_num, bool led_state);
void panel_adc_cb(float adc_value);
uint8_t rx_buffer[512] = {0};

void panel_key6_init(void)
{
    panel_gpio_init();
    app_adc_init(2);
    app_adc_callback(panel_adc_cb);     // 注册ADC回调函数
    app_slow_ctrl_led(PA8, 10, true);
    
}

void panel_gpio_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);

    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_8);    // 背光灯 GPIOA8

    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_15);   // 6个白灯
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_3);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_4);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_5);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_6);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_8);

    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_12);    // 4个继电器
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_13);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_14);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_15);

}
void panel_adc_cb(float adc_value)
{
    for (int i = 0; i < KEY_NUMBER_COUNT; i++) 
    {
        if (adc_value >= my_key_config[i].min && adc_value <= my_key_config[i].max) 
        {
            adc_value_buffer[adc_index++] = adc_value;
            
            if (adc_index >= ADC_VALUE_COUNT) 
            {
                adc_index = 0;
                buffer_full = 1;
                
                float new_value = app_calculate_average(adc_value_buffer, ADC_VALUE_COUNT);
                if (new_value >= my_key_config[i].min && new_value <= my_key_config[i].max) 
                {
                    if(my_panel_status[i].key_status == false)
                    {
                        APP_PRINTF("key [%d] down\n", i + 1);
                        my_panel_status[i].led_w_status = !my_panel_status[i].led_w_status;
                        panel_ctrl_led(i + 1, my_panel_status[i].led_w_status);
                        my_panel_status[i].key_status = true; // 标记为已按下
                    }
                }
            }
        }
        else
        {
            my_panel_status[i].key_status = false; // 标记为未按下
        }
    }
}

void panel_ctrl_led(uint8_t led_num, bool led_state)
{
    // 在此将按键号和 led 与 eraly 的 gpio 绑定
    GPIO_PinTypeDef led_gpio = GPIO_DEFAULT;
    GPIO_PinTypeDef eraly_gpio = GPIO_DEFAULT;
    switch (led_num)
    {
        case 1: 
            led_gpio = PA15;
            eraly_gpio = PB12;
            const char *cmd = "AT+SEND=FFFFFFFFFFFF,16,\"F10E010000002100\",1\r\n"; 
             
            app_usart0_send_string(cmd);
            APP_PRINTF("cmd:%s", cmd);
            break;
        case 2: 
            led_gpio = PB3;
            eraly_gpio = PB13;
            break;
        case 3: 
            led_gpio = PB4;
            eraly_gpio = PB14;
            break;
        case 4: 
            led_gpio = PB5;
            eraly_gpio = PB15;
            break;
        case 5: 
            led_gpio = PB6; 
            break;
        case 6: 
            led_gpio = PB8; 
            break;
            default: return;
    }
    app_ctrl_gpio(led_gpio, led_state);
    app_ctrl_gpio(eraly_gpio, led_state);
}






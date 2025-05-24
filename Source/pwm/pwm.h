#ifndef _LED_H_
#define _LED_H_

#include "../gpio/gpio.h"
#include "../timer/timer.h"

/*
    2025.5.24 舒东升
    用 TIMER_13 模拟 PWM 输出
    (原有的timer定时器用的是TIMER_14,adc和usart都使用了,
    过于冗长,在模拟PWM调光的时候有闪烁,故用TIMER_13专门模拟PWM输出)
*/

void app_pwm_init(void);
void set_pwm_duty(uint8_t duty);
void pwm_enable(void);

#if 0
typedef struct
{
    uint8_t pwm_counter;
    uint8_t pwm_duty;           // 当前占空比 (0-100)
    uint8_t target_duty;        // 目标占空比 (0-100)
    uint8_t dither_accumulator; // 抖动累加器
    uint8_t pwm_cycle;          // PWM 周期
    uint16_t fade_steps;        // 渐变总步数
    uint16_t current_step;      // 当前步数
    gpio_pin_typedef_t gpio;    // 输出的管脚
} pwm_status_t;


void pwm_state_init(pwm_status_t *state);
void app_pwm_ctrl(pwm_status_t *my_pwm_status, gpio_pin_typedef_t *gpio, uint8_t target_duty, uint16_t fade_time_ms);
#endif
#endif
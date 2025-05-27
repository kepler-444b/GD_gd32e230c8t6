#ifndef __PWM_H
#define __PWM_H

#include <stdint.h>
#include <stdbool.h>
#include "../gpio/gpio.h"

/* 
    2025.5.27 舒东升
    利用TIMER_13中断触发定时器,实现PWM输出,
    可以支持4路PWM输出(如有需要,还能继续添加)
*/

// PWM通道定义
typedef enum {
    PWM_CHANNEL_1 = 0,
    PWM_CHANNEL_2,
    PWM_CHANNEL_3,
    PWM_CHANNEL_4,
    PWM_CHANNEL_MAX
} pwm_channel_t;

// 初始化PWM模块,并指定输出引脚
void app_pwm_init(gpio_pin_typedef_t c1, gpio_pin_typedef_t c2, gpio_pin_typedef_t c3, gpio_pin_typedef_t c4);

// 设置指定通道的PWM占空比(0-500)，立即切换
void app_set_pwm_duty(pwm_channel_t channel, uint16_t duty);

// 设置指定通道的PWM渐变到指定占空比(0-500)，并指定渐变时间(ms)
void app_set_pwm_fade(pwm_channel_t channel, uint16_t duty, uint16_t fade_time_ms);

// 获取指定通道的当前PWM占空比
uint16_t app_get_pwm_duty(pwm_channel_t channel);

// 检查指定通道是否正在渐变中
bool app_is_pwm_fading(pwm_channel_t channel);

void app_pwm_poll(void);  // 用于主循环调用的渐变更新函数

#endif // __PWM_H
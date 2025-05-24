#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "pwm.h"
#include "gd32e23x_dbg.h"
#include "../gpio/gpio.h"

#define SYSTEM_CLOCK_FREQ 72000000 ///< 系统时钟频率
#define TIMER_PERIOD      999      ///< 定时器周期(1ms中断)
#define PWM_RESOLUTION    100      ///< PWM分辨率(100级)

// PWM控制变量
uint8_t pwm_duty = 0;       // 当前PWM占空比(0-100)
uint16_t pwm_counter = 0;   // PWM计数器(需要更大的范围)
bool pwm_output = false;    // PWM输出状态

// 初始化PWM调光
void app_pwm_init(void)
{
    timer_parameter_struct timer_initpara;

    rcu_periph_clock_enable(RCU_TIMER13); // 开启TIMER13时钟
    timer_deinit(TIMER13);                // 复位TIMER13
    
    // 配置定时器为100us中断(10倍PWM频率)
    timer_initpara.prescaler        = (SYSTEM_CLOCK_FREQ / 1000000) - 1; // 分频到1MHz
    timer_initpara.alignedmode      = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection = TIMER_COUNTER_UP;
    timer_initpara.period           = 99; // 100次计数 = 100us (10kHz中断)
    timer_initpara.clockdivision    = TIMER_CKDIV_DIV1;
    timer_init(TIMER13, &timer_initpara);
    
    // 使能中断
    timer_interrupt_flag_clear(TIMER13, TIMER_INT_FLAG_UP);
    timer_interrupt_enable(TIMER13, TIMER_INT_UP);
    timer_enable(TIMER13);
    nvic_irq_enable(TIMER13_IRQn, 1); // 配置NVIC,优先级1
    
    // 初始化PWM参数
    pwm_duty = 0;
    pwm_counter = 0;
    pwm_output = false;
}

// 设置PWM占空比(0-100)
void set_pwm_duty(uint8_t duty)
{
    if(duty > PWM_RESOLUTION) {
        duty = PWM_RESOLUTION;
    }
    pwm_duty = duty;
}

// 定时器中断处理函数
void TIMER13_IRQHandler(void)
{
    if (timer_interrupt_flag_get(TIMER13, TIMER_INT_FLAG_UP)) {
        timer_interrupt_flag_clear(TIMER13, TIMER_INT_FLAG_UP);
        
        // 1kHz PWM生成逻辑
        pwm_counter++;
        if(pwm_counter >= PWM_RESOLUTION) {
            pwm_counter = 0;
        }
        
        // 更新输出状态
        pwm_output = (pwm_counter < pwm_duty);
        app_ctrl_gpio(PA8, pwm_output);
    }
}
#if 0
void app_pwm_update(void *arg);

// 初始化PWM状态
// void pwm_state_init(pwm_status_t *state)
// {
//     state->pwm_counter        = 0;
//     state->pwm_duty           = 0;
//     state->target_duty        = 0;
//     state->dither_accumulator = 0;
//     state->pwm_cycle          = 1;   // 默认周期
//     state->fade_steps         = 100; // 默认渐变步数
//     state->current_step       = 0;
// }

// 设置LED渐变效果
void app_pwm_ctrl(pwm_status_t *my_pwm_status, gpio_pin_typedef_t *gpio, uint8_t target_duty, uint16_t fade_time_ms)
{
    // 计算需要的步数 (假设定时器周期为1ms)
    my_pwm_status->fade_steps   = fade_time_ms;
    my_pwm_status->current_step = 0;
    my_pwm_status->target_duty  = target_duty;
    my_pwm_status->gpio         = *gpio;
    
    if (app_timer_start(1, app_pwm_update, true, my_pwm_status, "pwm_ctrl") != TIMER_ERR_SUCCESS) {
        APP_ERROR("led_fade_timer");
    }
}

// PWM更新函数
void app_pwm_update(void *arg)
{
    pwm_status_t *my_pwm_status = (pwm_status_t*)arg;

    // 更新PWM计数器
    my_pwm_status->pwm_counter++;
    if (my_pwm_status->pwm_counter >= my_pwm_status->pwm_cycle) {
        my_pwm_status->pwm_counter = 0;

        // 渐变逻辑
        if (my_pwm_status->current_step < my_pwm_status->fade_steps) {
            my_pwm_status->current_step++;

            // 线性渐变计算
            int16_t new_duty = (int16_t)my_pwm_status->target_duty * my_pwm_status->current_step / my_pwm_status->fade_steps;

            // 确保值在0-100范围内
            if (new_duty < 0) new_duty = 0;
            if (new_duty > 100) new_duty = 100;

            my_pwm_status->pwm_duty = (uint8_t)new_duty;
        }
    }

    // 抖动算法
    my_pwm_status->dither_accumulator += my_pwm_status->pwm_duty;
    bool led_on = (my_pwm_status->dither_accumulator >= 100);
    if (led_on) {
        my_pwm_status->dither_accumulator -= 100;
    }

    // 控制GPIO
    app_ctrl_gpio(my_pwm_status->gpio, led_on);
}

#endif

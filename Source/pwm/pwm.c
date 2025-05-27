#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "pwm.h"
#include "curve_table.h"
#include "gd32e23x_dbg.h"
#include "../gpio/gpio.h"

// #define PWM_DIR           // pwm 方向(开启次宏为反,关闭为正)

#define SYSTEM_CLOCK_FREQ 72000000 // 系统时钟频率(72MHz)
#define TIMER_PERIOD      14       // 定时器周期(15us中断,不可再低)
#define PWM_RESOLUTION    500      // PWM分辨率(500)
#define MAX_FADE_TIME_MS  5000     // 最大渐变时间(5秒)
#define FADE_UPDATE_MS    10       // 渐变更新间隔(10ms)

// 全局变量用于主循环处理
static volatile uint16_t pwm_counter = 0;
static volatile uint16_t fade_timer  = 0;
static volatile bool pwm_update_flag = false;

// PWM控制结构体
typedef struct {
    uint16_t current_duty;   // 当前PWM占空比(0-1000)
    uint16_t target_duty;    // 目标PWM占空比(0-1000)
    uint16_t fade_counter;   // 渐变计数器
    uint16_t fade_steps;     // 渐变总步数
    uint16_t start_duty;     // 渐变起始占空比
    bool is_fading;          // 是否正在渐变中
    gpio_pin_typedef_t gpio; // 对应的GPIO引脚
} pwm_control_t;

// PWM通道控制数组
static pwm_control_t pwm_channels[PWM_CHANNEL_MAX];

// 初始化PWM调光
void app_pwm_init(gpio_pin_typedef_t c1, gpio_pin_typedef_t c2, gpio_pin_typedef_t c3, gpio_pin_typedef_t c4)
{
    // 初始化通道引脚映射
    pwm_channels[0].gpio = c1; // 通道1: PB7
    pwm_channels[1].gpio = c2; // 通道2: PB5
    pwm_channels[2].gpio = c3; // 通道3: PB6
    pwm_channels[3].gpio = c4; // 通道4: PB4

    // 初始化所有PWM通道参数
    for (int i = 0; i < PWM_CHANNEL_MAX; i++) {
        pwm_channels[i].current_duty = 0;
        pwm_channels[i].target_duty  = 0;
        pwm_channels[i].fade_counter = 0;
        pwm_channels[i].fade_steps   = 0;
        pwm_channels[i].is_fading    = false;

        app_ctrl_gpio(pwm_channels[i].gpio, true); // 初始状态关闭
    }

    timer_parameter_struct timer_initpara;

    // 配置定时器
    rcu_periph_clock_enable(RCU_TIMER13); // 开启TIMER13时钟
    timer_deinit(TIMER13);                // 复位TIMER13

    timer_initpara.prescaler        = (SYSTEM_CLOCK_FREQ / 1000000) - 1; // 分频到1MHz
    timer_initpara.alignedmode      = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection = TIMER_COUNTER_UP;
    timer_initpara.period           = TIMER_PERIOD;
    timer_initpara.clockdivision    = TIMER_CKDIV_DIV1;
    timer_init(TIMER13, &timer_initpara);

    // 使能中断
    timer_interrupt_flag_clear(TIMER13, TIMER_INT_FLAG_UP);
    timer_interrupt_enable(TIMER13, TIMER_INT_UP);
    timer_enable(TIMER13);
    nvic_irq_enable(TIMER13_IRQn, 1); // 配置NVIC,优先级1
}

void app_set_pwm_duty(pwm_channel_t channel, uint16_t duty)
{
    if (channel >= PWM_CHANNEL_MAX) return;

    if (duty > PWM_RESOLUTION) {
        duty = PWM_RESOLUTION;
    }

    pwm_channels[channel].current_duty = duty;
    pwm_channels[channel].target_duty  = duty;
    pwm_channels[channel].is_fading    = false;
}

void app_set_pwm_fade(pwm_channel_t channel, uint16_t duty, uint16_t fade_time_ms)
{
    if (channel >= PWM_CHANNEL_MAX) return;

    // 限制占空比范围
    duty = (duty > PWM_RESOLUTION) ? PWM_RESOLUTION : duty;

    // 渐变时间为0则立即切换
    if (fade_time_ms == 0) {
        app_set_pwm_duty(channel, duty);
        return;
    }

    // 限制最大渐变时间
    fade_time_ms = (fade_time_ms > MAX_FADE_TIME_MS) ? MAX_FADE_TIME_MS : fade_time_ms;

    // 计算渐变步数(带四舍五入)
    uint16_t steps = (fade_time_ms + FADE_UPDATE_MS / 2) / FADE_UPDATE_MS;
    steps          = (steps == 0) ? 1 : steps; // 确保至少1步

    // 更新渐变参数
    pwm_channels[channel].target_duty  = duty;
    pwm_channels[channel].start_duty   = pwm_channels[channel].current_duty;
    pwm_channels[channel].fade_steps   = steps;
    pwm_channels[channel].fade_counter = 0;
    pwm_channels[channel].is_fading    = true;

    // 如果当前已在目标值，直接完成渐变
    if (pwm_channels[channel].current_duty == duty) {
        pwm_channels[channel].is_fading = false;
    }
}

// 获取指定通道的当前PWM占空比
uint16_t app_get_pwm_duty(pwm_channel_t channel)
{
    if (channel >= PWM_CHANNEL_MAX) return 0;
    return pwm_channels[channel].current_duty;
}

// 检查指定通道是否正在渐变中
bool app_is_pwm_fading(pwm_channel_t channel)
{
    if (channel >= PWM_CHANNEL_MAX) return false;
    return pwm_channels[channel].is_fading;
}

// 定时器中断处理函数
void TIMER13_IRQHandler(void)
{
#if 1 // 在主循环中处理
    if (timer_interrupt_flag_get(TIMER13, TIMER_INT_FLAG_UP)) {
        timer_interrupt_flag_clear(TIMER13, TIMER_INT_FLAG_UP);

        // PWM计数器更新
        pwm_counter++;
        if (pwm_counter >= PWM_RESOLUTION) {
            pwm_counter = 0;
        }

        // 渐变计时器更新
        fade_timer++;
        if (fade_timer >= 660) { // 15us * 660 ≈ 10ms
            fade_timer      = 0;
            pwm_update_flag = true; // 通知主循环处理渐变
        }

        // 立即更新PWM输出
        for (int i = 0; i < PWM_CHANNEL_MAX; i++) {
            bool pwm_output = (pwm_counter < pwm_channels[i].current_duty);
#if defined PWM_DIR
            app_ctrl_gpio(pwm_channels[i].gpio, !pwm_output);
#else
            app_ctrl_gpio(pwm_channels[i].gpio, pwm_output);
#endif
        }
    }
#endif
#if 0 // 在中断中处理
    static uint16_t pwm_counter = 0;
    static uint16_t fade_timer  = 0;

    if (timer_interrupt_flag_get(TIMER13, TIMER_INT_FLAG_UP)) {
        timer_interrupt_flag_clear(TIMER13, TIMER_INT_FLAG_UP);

        // PWM生成逻辑
        pwm_counter++;
        if (pwm_counter >= PWM_RESOLUTION) {
            pwm_counter = 0;
        }

        // 更新所有通道的输出状态
        for (int i = 0; i < PWM_CHANNEL_MAX; i++) {
            bool pwm_output = (pwm_counter < pwm_channels[i].current_duty);
#if defined PWM_DIR
            app_ctrl_gpio(pwm_channels[i].gpio, !pwm_output);
#else
            app_ctrl_gpio(pwm_channels[i].gpio, pwm_output);
#endif
        }

        // 渐变处理逻辑 (每10ms调整一次占空比)
        fade_timer++;
        if (fade_timer >= 660) { // 15us * 660 ≈ 10ms
            fade_timer = 0;

            for (int i = 0; i < PWM_CHANNEL_MAX; i++) {
                if (pwm_channels[i].is_fading) {
                    pwm_channels[i].fade_counter++;

                    // 计算当前进度(0-FADE_TABLE_SIZE)
                    uint32_t progress = (uint32_t)pwm_channels[i].fade_counter * FADE_TABLE_SIZE / pwm_channels[i].fade_steps;
                    progress          = (progress >= FADE_TABLE_SIZE) ? FADE_TABLE_SIZE - 1 : progress;

                    // 获取缓入缓出曲线值(0-500)
                    uint16_t curve_value = fade_table[progress];

                    // 计算当前占空比
                    int32_t start = pwm_channels[i].start_duty;
                    int32_t end   = pwm_channels[i].target_duty;
                    int32_t range = end - start;

                    // 应用缓入缓出曲线
                    int32_t new_duty;
                    if (range >= 0) {
                        new_duty = start + (range * curve_value) / 500;
                    } else {
                        new_duty = start - ((-range) * curve_value) / 500;
                    }

                    // 确保占空比在有效范围内
                    new_duty                     = (new_duty < 0) ? 0 : new_duty;
                    new_duty                     = (new_duty > PWM_RESOLUTION) ? PWM_RESOLUTION : new_duty;
                    pwm_channels[i].current_duty = (uint16_t)new_duty;

                    // 渐变完成检查
                    if (pwm_channels[i].fade_counter >= pwm_channels[i].fade_steps) {
                        pwm_channels[i].current_duty = end;
                        pwm_channels[i].is_fading    = false;
                    }
                }
            }
        }
    }
#endif
}

// 在住循环中运行(由于速度不够快，暂时弃用)
#if 1
void app_pwm_poll(void)
{
    if (pwm_update_flag) {
        pwm_update_flag = false;

        // 处理所有通道的渐变逻辑
        for (int i = 0; i < PWM_CHANNEL_MAX; i++) {
            if (pwm_channels[i].is_fading) {
                pwm_channels[i].fade_counter++;

                // 计算当前进度(0-FADE_TABLE_SIZE)
                uint32_t progress = (uint32_t)pwm_channels[i].fade_counter * FADE_TABLE_SIZE / pwm_channels[i].fade_steps;
                progress          = (progress >= FADE_TABLE_SIZE) ? FADE_TABLE_SIZE - 1 : progress;

                // 获取缓入缓出曲线值(0-500)
                uint16_t curve_value = fade_table[progress];

                // 计算当前占空比
                int32_t start = pwm_channels[i].start_duty;
                int32_t end   = pwm_channels[i].target_duty;
                int32_t range = end - start;

                // 应用缓入缓出曲线
                int32_t new_duty;
                if (range >= 0) {
                    new_duty = start + (range * curve_value) / 500;
                } else {
                    new_duty = start - ((-range) * curve_value) / 500;
                }

                // 确保占空比在有效范围内
                new_duty                     = (new_duty < 0) ? 0 : new_duty;
                new_duty                     = (new_duty > PWM_RESOLUTION) ? PWM_RESOLUTION : new_duty;
                pwm_channels[i].current_duty = (uint16_t)new_duty;

                // 渐变完成检查
                if (pwm_channels[i].fade_counter >= pwm_channels[i].fade_steps) {
                    pwm_channels[i].current_duty = end;
                    pwm_channels[i].is_fading    = false;
                }
            }
        }
    }
}
#endif
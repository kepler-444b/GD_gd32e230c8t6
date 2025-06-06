#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "pwm.h"
#include "curve_table.h"
#include "gd32e23x_dbg.h"
#include "../gpio/gpio.h"
#include "../device/device_manager.h"

#define SYSTEM_CLOCK_FREQ 72000000 // 系统时钟频率(72MHz)
#define TIMER_PERIOD      14       // 定时器周期(15us中断,不可再低)
#define PWM_RESOLUTION    500      // PWM分辨率(500)
#define MAX_FADE_TIME_MS  5000     // 最大渐变时间(5秒)
#define FADE_UPDATE_MS    10       // 渐变更新间隔(10ms)

// 定义 MIN 宏(取较小值)
#define MIN(a, b) ((a) < (b) ? (a) : (b))
// 定义 CLAMP 宏(限制值在 [min_val, max_val] 范围内)
#define CLAMP(x, min_val, max_val) ((x) < (min_val) ? (min_val) : ((x) > (max_val) ? (max_val) : (x)))

// 全局变量用于主循环处理
#if 0
static volatile uint16_t pwm_counter = 0;
static volatile uint16_t fade_timer  = 0;
static volatile bool pwm_update_flag = false;
#endif

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
static uint8_t active_channel_count = PWM_CHANNEL_MAX;

// 初始化PWM调光
void app_pwm_init(gpio_pin_typedef_t *pwm_channel_pins, uint8_t channel_count)
{
    for (int i = 0; i < channel_count; i++) {
        pwm_channels[i].current_duty = 0;
        pwm_channels[i].target_duty  = 0;
        pwm_channels[i].fade_counter = 0;
        pwm_channels[i].fade_steps   = 0;
        pwm_channels[i].is_fading    = false;
        pwm_channels[i].gpio         = pwm_channel_pins[i];

        APP_SET_GPIO(pwm_channels[i].gpio, true); // 初始状态关闭
    }
    active_channel_count = MIN(channel_count, PWM_CHANNEL_MAX);

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
    // APP_PRINTF("app_pwm_init\n");
}

void app_set_pwm_duty(pwm_channel_t channel, uint16_t duty)
{
    if (channel >= (active_channel_count)) return;

    if (duty > PWM_RESOLUTION) {
        duty = PWM_RESOLUTION;
    }

    pwm_channels[channel].current_duty = duty;
    pwm_channels[channel].target_duty  = duty;
    pwm_channels[channel].is_fading    = false;
}

void app_set_pwm_fade(pwm_channel_t channel, uint16_t duty, uint16_t fade_time_ms)
{
    if (channel >= (active_channel_count)) return;

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
    if (channel >= (active_channel_count)) return 0;
    return pwm_channels[channel].current_duty;
}

// 检查指定通道是否正在渐变中
bool app_is_pwm_fading(pwm_channel_t channel)
{
    if (channel >= (active_channel_count)) return false;
    return pwm_channels[channel].is_fading;
}

// 定时器中断处理函数
void TIMER13_IRQHandler(void)
{
#if 0 // 在主循环中处理
    static uint16_t pwm_counter = 0; // 静态变量保证状态持久

    if (timer_interrupt_flag_get(TIMER13, TIMER_INT_FLAG_UP)) {
        timer_interrupt_flag_clear(TIMER13, TIMER_INT_FLAG_UP);

        // PWM计数器更新
        pwm_counter++;
        if (pwm_counter >= PWM_RESOLUTION) {
            pwm_counter = 0;
        }

        // 实时更新所有通道输出状态
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
#if 1 // 在中断中处理
    static uint16_t pwm_counter = 0;
    static uint16_t fade_timer  = 0;

    if (timer_interrupt_flag_get(TIMER13, TIMER_INT_FLAG_UP)) {
        timer_interrupt_flag_clear(TIMER13, TIMER_INT_FLAG_UP);

        // PWM生成逻辑
        pwm_counter = (pwm_counter + 1) % PWM_RESOLUTION;

        // 更新所有通道的输出状态
        for (uint8_t i = 0; i < (active_channel_count); i++) { // 只轮询活跃的PWM
            bool pwm_output = (pwm_counter < pwm_channels[i].current_duty);
            APP_SET_GPIO(pwm_channels[i].gpio,
#if defined PWM_DIR
                          !pwm_output
#else
                          pwm_output
#endif
            );
        }

        // 渐变处理逻辑(每10ms调整一次占空比)
        if ((fade_timer = (fade_timer + 1) % 660) == 0) {
            for (int i = 0; i < (active_channel_count); i++) { // 只轮询活跃的PWM
                if (!pwm_channels[i].is_fading) continue;

                pwm_channels[i].fade_counter++;

                // 计算进度并限制范围
                uint32_t progress = (uint32_t)pwm_channels[i].fade_counter * FADE_TABLE_SIZE /
                                    pwm_channels[i].fade_steps;
                progress = MIN(progress, FADE_TABLE_SIZE - 1);

                // 获取缓入缓出曲线值
                uint16_t curve_value = fade_table[progress];

                // 计算新占空比
                int32_t start = pwm_channels[i].start_duty;
                int32_t end   = pwm_channels[i].target_duty;
                int32_t range = end - start;

                int32_t new_duty = start + (range * curve_value) / 500;
                new_duty         = CLAMP(new_duty, 0, PWM_RESOLUTION);

                pwm_channels[i].current_duty = (uint16_t)new_duty;

                // 检查渐变是否完成
                if (pwm_channels[i].fade_counter >= pwm_channels[i].fade_steps) {
                    pwm_channels[i].current_duty = end;
                    pwm_channels[i].is_fading    = false;
                }
            }
        }
    }
#endif
}

#if 0
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
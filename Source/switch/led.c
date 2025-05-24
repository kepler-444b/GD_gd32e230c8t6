#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "led.h"
#include "gd32e23x_dbg.h"
#include "../timer/timer.h"
#include "../gpio/gpio.h"

typedef struct {
    uint8_t current_brightness;  // 当前亮度 (0-100)
    uint8_t target_brightness;   // 目标亮度 (0-100)
    uint16_t transition_ms;      // 渐变总时长(毫秒)
    uint16_t elapsed_ms;         // 已过去的时间(毫秒)
    uint16_t update_interval_ms; // 更新间隔(毫秒)
    gpio_pin_typedef_t gpio;     // 输出的管脚
    uint8_t dither_accumulator;  // 抖动累加器
    bool is_active;              // 是否正在渐变中
} pwm_state;

static pwm_state my_pwm_state;

void app_update_led_pwm(void *arg);


// 初始化LED控制
void app_led_init(gpio_pin_typedef_t gpio, uint16_t update_interval_ms) {
    memset(&my_pwm_state, 0, sizeof(pwm_state));
    my_pwm_state.gpio = gpio;
    my_pwm_state.update_interval_ms = update_interval_ms;
}

// 设置LED亮度(带渐变效果)
void app_set_led_brightness(uint8_t brightness, uint16_t transition_ms) {
    // 如果渐变时间为0，则立即设置
    if (transition_ms == 0) {
        my_pwm_state.current_brightness = brightness;
        my_pwm_state.target_brightness = brightness;
        my_pwm_state.is_active = false;
        app_update_led_pwm(NULL);  // 立即更新
        return;
    }
    
    // 设置渐变参数
    my_pwm_state.target_brightness = brightness;
    my_pwm_state.transition_ms = transition_ms;
    my_pwm_state.elapsed_ms = 0;
    my_pwm_state.is_active = true;
    
    // 启动定时器
    if (app_timer_start(my_pwm_state.update_interval_ms, app_update_led_pwm, true, NULL, "led_pwm_timer") != TIMER_ERR_SUCCESS) {
        APP_ERROR("Failed to start LED PWM timer");
    }
}

// LED PWM更新回调函数
void app_update_led_pwm(void *arg) 
{
    (void)arg;  // 未使用参数
    
    // 如果不需要渐变，停止定时器
    if (!my_pwm_state.is_active || 
        my_pwm_state.current_brightness == my_pwm_state.target_brightness) {
        app_timer_stop("led_pwm_timer");
        my_pwm_state.is_active = false;
        return;
    }
    
    // 更新已过去的时间
    my_pwm_state.elapsed_ms += my_pwm_state.update_interval_ms;
    
    // 计算当前亮度(线性插值)
    if (my_pwm_state.elapsed_ms >= my_pwm_state.transition_ms) {
        // 渐变完成
        my_pwm_state.current_brightness = my_pwm_state.target_brightness;
        my_pwm_state.is_active = false;
    } else {
        // 计算中间亮度
        int16_t delta = (int16_t)my_pwm_state.target_brightness - (int16_t)my_pwm_state.current_brightness;
        uint16_t progress = (my_pwm_state.elapsed_ms * 1000) / my_pwm_state.transition_ms; // 0-1000
        int16_t increment = (delta * progress) / 1000;
        my_pwm_state.current_brightness += increment;
        
        // 确保亮度在0-100范围内
        if (my_pwm_state.current_brightness > 100) my_pwm_state.current_brightness = 100;
        if (my_pwm_state.current_brightness < 0) my_pwm_state.current_brightness = 0;
    }
    
    // 应用抖动算法实现PWM
    my_pwm_state.dither_accumulator += my_pwm_state.current_brightness;
    bool led_on = (my_pwm_state.dither_accumulator >= 100);
    if (led_on) my_pwm_state.dither_accumulator -= 100;
    
    // 控制GPIO
    app_ctrl_gpio(my_pwm_state.gpio, led_on);
}
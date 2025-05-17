#include "gd32e23x.h"
#include "base.h"
#include <stdio.h>
#include <float.h>
#include "../timer/timer.h"

typedef struct
{
    uint8_t pwm_counter;
    uint8_t pwm_duty;
    uint8_t dither_accumulator;  // 抖动累加器
    uint8_t pwm_cycle;           // PWM 周期
    GPIO_PinTypeDef gpio;        // 输出的管脚
}pwm_state;
static pwm_state my_pwm_state;


typedef struct
{
    uint8_t timer_1;
    uint8_t timer_2;
}timer_id;
static timer_id my_timer_id = {0, 0};

void app_start_close_led(void);
void app_start_open_led(void);
float app_calculate_average(const float *buffer, uint16_t count) 
{
    // 参数检查
    if (buffer == NULL || count == 0) {
        return FLT_MAX;  // 或返回 0.0f，根据需求决定
    }

    float sum = 0.0f;
    for (uint16_t i = 0; i < count; i++) 
    {
        sum += buffer[i];
    }

    // 避免除以零（虽然前面已检查 count>0）
    return (count != 0) ? (sum / (float)count) : FLT_MAX;
}

void app_slow_ctrl_led(GPIO_PinTypeDef gpio, uint8_t pwm_cycle, bool status)
{ 
    my_pwm_state .pwm_cycle = pwm_cycle;
    my_pwm_state.gpio = gpio;
    if(status)
    {
        
        my_timer_id.timer_1 = app_timer_start(1, app_start_open_led, true);
    }
    else 
    {
        my_timer_id.timer_2 = app_timer_start(1, app_start_close_led, true);
    }
}

void app_start_open_led(void)
{
    my_pwm_state.pwm_counter++;
    if (my_pwm_state.pwm_counter >= my_pwm_state.pwm_cycle) {
        my_pwm_state.pwm_counter = 0;
        if (my_pwm_state.pwm_duty < 100) 
        {
            my_pwm_state.pwm_duty ++;
        }
        if(my_pwm_state.pwm_duty >= 100)
        {
            app_timer_stop(my_timer_id.timer_1);
        }
    }

    // 抖动算法：通过累加占空比，分散亮灭脉冲
    my_pwm_state.dither_accumulator += my_pwm_state.pwm_duty;
    bool led_on = (my_pwm_state.dither_accumulator >= 100);
    if (led_on) my_pwm_state.dither_accumulator -= 100;

    app_ctrl_gpio(PA8, led_on);
}

void app_start_close_led(void)
{
    my_pwm_state.pwm_counter++;
    if (my_pwm_state.pwm_counter >= my_pwm_state.pwm_cycle) {
        my_pwm_state.pwm_counter = 0;
        if (my_pwm_state.pwm_duty > 0) 
        {
            my_pwm_state.pwm_duty--;  // 每个周期减少占空比
        }
    }
    
    // 抖动算法：通过累加占空比，分散亮灭脉冲
    my_pwm_state.dither_accumulator += my_pwm_state.pwm_duty;
    bool led_on = (my_pwm_state.dither_accumulator >= 100);
    if (led_on) my_pwm_state.dither_accumulator -= 100;

    app_ctrl_gpio(PA8, led_on);
}
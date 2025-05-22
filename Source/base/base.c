#include "gd32e23x.h"
#include "base.h"
#include <stdio.h>
#include <float.h>
#include "../timer/timer.h"


timer_id my_timer_id = {0, 0, 0, 0};
dev_config my_dev_config = {0};

typedef struct
{
    uint8_t pwm_counter;
    uint8_t pwm_duty;
    uint8_t dither_accumulator;  // 抖动累加器
    uint8_t pwm_cycle;           // PWM 周期
    GPIO_PinTypeDef gpio;        // 输出的管脚
}pwm_state;
static pwm_state my_pwm_state;



void app_start_close_led(void *arg);
void app_start_open_led(void* arg);
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
        
        my_timer_id.timer_1 = app_timer_start(1, app_start_open_led, true, NULL);
    }
    else 
    {
        my_timer_id.timer_2 = app_timer_start(1, app_start_close_led, true, NULL);
    }
}

void app_start_open_led(void* arg)
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

void app_start_close_led(void *arg)
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

bool app_uint8_to_uint32(const uint8_t* input, size_t input_count, uint32_t* output, size_t output_count) 
{
    if (!input || !output) return false;
    if (output_count < (input_count + 3) / 4) 
    {
        return false;
    }

    for (size_t i = 0; i < (input_count + 3) / 4; i++) {
        uint32_t packed = 0;
        size_t bytes_to_pack = (input_count - i * 4) >= 4 ? 4 : (input_count - i * 4);
        
        for (size_t j = 0; j < bytes_to_pack; j++) {
            packed |= (uint32_t)input[i * 4 + j] << (j * 8);  // 小端序
        }
        output[i] = packed;
    }
    return true;
}

bool app_uint32_to_uint8(const uint32_t* input, size_t input_count, uint8_t* output, size_t output_count) 
{
    if (!input || !output) return false;
    if (output_count < input_count * 4) 
    {
        return false;
    }

    for (size_t i = 0; i < input_count; i++) {
        for (size_t j = 0; j < 4; j++) {
            output[i * 4 + j] = (uint8_t)((input[i] >> (j * 8)) & 0xFF);  // 小端序
        }
    }
    return true;
}


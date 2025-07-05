#ifndef _ZERO_H_
#define _ZERO_H_

#include "gd32e23x.h"
#include <stdbool.h>
#include "../gpio/gpio.h"

/* 
    2025.7.5 舒东升
    零点检测模块,可使管脚精准在零点时动作
    
*/

// 初始化过零检测(exti_line_enum 编号根据GPIO引脚确定,参考"数据手册5.5")
void app_zero_init(exti_line_enum exti_pin);

// 使用此函数设置的GPIO管脚,会在零点后延时若干ms后触发
void zero_set_gpio(const gpio_pin_t pin, bool status);

#endif
#ifndef _BASE_H_
#define _BASE_H_ 
#include "../gpio/gpio.h"
/* 
    2025.5.16 舒东升
    本模块用于各种通用接口
*/


/// @brief 用于求一组数据的平均数
/// @param buffer 传入数组
/// @param count  数组长度
/// @return 
float app_calculate_average(const float *buffer, uint16_t count);


/// @brief 控制一个gpio口模拟pwm输出
/// @param gpio       输出的gpio口
/// @param pwm_cycle  周期
/// @param status     输出状态
void app_slow_ctrl_led(GPIO_PinTypeDef gpio, uint8_t pwm_cycle, bool status);
#endif
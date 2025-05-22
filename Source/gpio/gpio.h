#ifndef _GPIO_H_
#define _GPIO_H_
#include "gd32e23x.h"
#include <stdbool.h>
#include "../base/debug.h"

#define GPIO_DEFAULT {0, 0}  // 0 表示未初始化或无效引脚
#define GPIO_IS_VALID(gpio) ((gpio).pin != 0 || (gpio).port != 0)  // 检查GPIO是否有效

typedef struct {
    uint32_t port;
    uint32_t pin;
} GPIO_PinTypeDef;

extern GPIO_PinTypeDef PA0;
extern GPIO_PinTypeDef PA1;
extern GPIO_PinTypeDef PA2;
extern GPIO_PinTypeDef PA3;
extern GPIO_PinTypeDef PA4;
extern GPIO_PinTypeDef PA5;
extern GPIO_PinTypeDef PA6;
extern GPIO_PinTypeDef PA7;
extern GPIO_PinTypeDef PA8;
extern GPIO_PinTypeDef PA9;
extern GPIO_PinTypeDef PA10;
extern GPIO_PinTypeDef PA11;
extern GPIO_PinTypeDef PA12;
extern GPIO_PinTypeDef PA13;
extern GPIO_PinTypeDef PA14;
extern GPIO_PinTypeDef PA15;
extern GPIO_PinTypeDef PB0;
extern GPIO_PinTypeDef PB1;
extern GPIO_PinTypeDef PB2;
extern GPIO_PinTypeDef PB3;
extern GPIO_PinTypeDef PB4;
extern GPIO_PinTypeDef PB5;
extern GPIO_PinTypeDef PB6;
extern GPIO_PinTypeDef PB7;
extern GPIO_PinTypeDef PB8;
extern GPIO_PinTypeDef PB9;
extern GPIO_PinTypeDef PB10;
extern GPIO_PinTypeDef PB11;
extern GPIO_PinTypeDef PB12;
extern GPIO_PinTypeDef PB13;
extern GPIO_PinTypeDef PB14;
extern GPIO_PinTypeDef PB15;



static inline void app_ctrl_gpio(GPIO_PinTypeDef gpio, bool status)
{
    if(gpio.port != -1 && gpio.pin != -1)
    {
        status ? gpio_bit_set(gpio.port, gpio.pin) : gpio_bit_reset(gpio.port, gpio.pin);
    }
}
#endif
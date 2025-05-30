#ifndef _GPIO_H_
#define _GPIO_H_
#include "gd32e23x.h"
#include <stdbool.h>
#include "../base/debug.h"

#define GPIO_DEFAULT        {0, 0}                                // 0 表示未初始化或无效引脚
#define GPIO_IS_VALID(gpio) ((gpio).pin != 0 || (gpio).port != 0) // 检查GPIO是否有效

typedef struct
{
    uint32_t port;
    uint32_t pin;
} gpio_pin_typedef_t;

extern gpio_pin_typedef_t DEFAULT;
extern gpio_pin_typedef_t PA0;
extern gpio_pin_typedef_t PA1;
extern gpio_pin_typedef_t PA2;
extern gpio_pin_typedef_t PA3;
extern gpio_pin_typedef_t PA4;
extern gpio_pin_typedef_t PA5;
extern gpio_pin_typedef_t PA6;
extern gpio_pin_typedef_t PA7;
extern gpio_pin_typedef_t PA8;
extern gpio_pin_typedef_t PA9;
extern gpio_pin_typedef_t PA10;
extern gpio_pin_typedef_t PA11;
extern gpio_pin_typedef_t PA12;
extern gpio_pin_typedef_t PA13;
extern gpio_pin_typedef_t PA14;
extern gpio_pin_typedef_t PA15;
extern gpio_pin_typedef_t PB0;
extern gpio_pin_typedef_t PB1;
extern gpio_pin_typedef_t PB2;
extern gpio_pin_typedef_t PB3;
extern gpio_pin_typedef_t PB4;
extern gpio_pin_typedef_t PB5;
extern gpio_pin_typedef_t PB6;
extern gpio_pin_typedef_t PB7;
extern gpio_pin_typedef_t PB8;
extern gpio_pin_typedef_t PB9;
extern gpio_pin_typedef_t PB10;
extern gpio_pin_typedef_t PB11;
extern gpio_pin_typedef_t PB12;
extern gpio_pin_typedef_t PB13;
extern gpio_pin_typedef_t PB14;
extern gpio_pin_typedef_t PB15;


inline void app_ctrl_gpio(gpio_pin_typedef_t gpio, bool status) {
    if (gpio.port != -1 && gpio.pin != -1) {
        status ? gpio_bit_set(gpio.port, gpio.pin) : gpio_bit_reset(gpio.port, gpio.pin);
    }
}
inline FlagStatus app_get_gpio(gpio_pin_typedef_t gpio)
{
    return gpio_input_bit_get(gpio.port, gpio.pin);
}

#endif
#ifndef _GPIO_H_
#define _GPIO_H_
#include "gd32e23x.h"
#include <stdbool.h>
#include "../base/debug.h"

/*
    2025.5.9 舒东升
    将GPIO操作抽象化,引入 gpio_pin_typedef_t 类型,
    将port和pin合为一个参数,使得操作更灵活
    利用宏定义
    APP_SET_GPIO 替换 gpio_bit_set
    APP_GET_GPIO 替换 gpio_input_bit_get
    在频繁操作GPIO的时候(如PWM)更加高效
*/

// 检查GPIO是否有效
#define GPIO_IS_VALID(obj) \
    ((obj).pin != 0 || (obj).port != 0)

// 设置GPIO
#define APP_SET_GPIO(obj, status) \
    ((status) ? (GPIO_BOP((obj).port) = (uint32_t)(obj).pin) : (GPIO_BC((obj).port) = (uint32_t)(obj).pin))

// 获取GPIO
#define APP_GET_GPIO(obj) \
    (((bool)false != (GPIO_ISTAT((obj).port) & ((obj).pin))) ? true : false)

typedef struct {
    uint32_t port;
    uint32_t pin;
} gpio_pin_typedef_t;

const char *app_get_gpio_name(gpio_pin_typedef_t gpio);

// 通用GPIO宏定义
#define DEFINE_GPIO(port, pin) ((gpio_pin_typedef_t){port, pin})

// 默认无效引脚
#define DEFAULT DEFINE_GPIO(0, 0)
#define PA0     DEFINE_GPIO(GPIOA, GPIO_PIN_0)
#define PA1     DEFINE_GPIO(GPIOA, GPIO_PIN_1)
#define PA2     DEFINE_GPIO(GPIOA, GPIO_PIN_2)
#define PA3     DEFINE_GPIO(GPIOA, GPIO_PIN_3)
#define PA4     DEFINE_GPIO(GPIOA, GPIO_PIN_4)
#define PA5     DEFINE_GPIO(GPIOA, GPIO_PIN_5)
#define PA6     DEFINE_GPIO(GPIOA, GPIO_PIN_6)
#define PA7     DEFINE_GPIO(GPIOA, GPIO_PIN_7)
#define PA8     DEFINE_GPIO(GPIOA, GPIO_PIN_8)
#define PA9     DEFINE_GPIO(GPIOA, GPIO_PIN_9)
#define PA10    DEFINE_GPIO(GPIOA, GPIO_PIN_10)
#define PA11    DEFINE_GPIO(GPIOA, GPIO_PIN_11)
#define PA12    DEFINE_GPIO(GPIOA, GPIO_PIN_12)
#define PA13    DEFINE_GPIO(GPIOA, GPIO_PIN_13)
#define PA14    DEFINE_GPIO(GPIOA, GPIO_PIN_14)
#define PA15    DEFINE_GPIO(GPIOA, GPIO_PIN_15)
#define PB0     DEFINE_GPIO(GPIOB, GPIO_PIN_0)
#define PB1     DEFINE_GPIO(GPIOB, GPIO_PIN_1)
#define PB2     DEFINE_GPIO(GPIOB, GPIO_PIN_2)
#define PB3     DEFINE_GPIO(GPIOB, GPIO_PIN_3)
#define PB4     DEFINE_GPIO(GPIOB, GPIO_PIN_4)
#define PB5     DEFINE_GPIO(GPIOB, GPIO_PIN_5)
#define PB6     DEFINE_GPIO(GPIOB, GPIO_PIN_6)
#define PB7     DEFINE_GPIO(GPIOB, GPIO_PIN_7)
#define PB8     DEFINE_GPIO(GPIOB, GPIO_PIN_8)
#define PB9     DEFINE_GPIO(GPIOB, GPIO_PIN_9)
#define PB10    DEFINE_GPIO(GPIOB, GPIO_PIN_10)
#define PB11    DEFINE_GPIO(GPIOB, GPIO_PIN_11)
#define PB12    DEFINE_GPIO(GPIOB, GPIO_PIN_12)
#define PB13    DEFINE_GPIO(GPIOB, GPIO_PIN_13)
#define PB14    DEFINE_GPIO(GPIOB, GPIO_PIN_14)
#define PB15    DEFINE_GPIO(GPIOB, GPIO_PIN_15)

#endif
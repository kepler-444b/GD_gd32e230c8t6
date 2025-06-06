#include "gpio.h"
#include <stdint.h>
#include <stdbool.h>
#include "gd32e23x.h"

gpio_pin_typedef_t DEFAULT = {0, 0};
gpio_pin_typedef_t PA0     = {GPIOA, GPIO_PIN_0};
gpio_pin_typedef_t PA1     = {GPIOA, GPIO_PIN_1};
gpio_pin_typedef_t PA2     = {GPIOA, GPIO_PIN_2};
gpio_pin_typedef_t PA3     = {GPIOA, GPIO_PIN_3};
gpio_pin_typedef_t PA4     = {GPIOA, GPIO_PIN_4};
gpio_pin_typedef_t PA5     = {GPIOA, GPIO_PIN_5};
gpio_pin_typedef_t PA6     = {GPIOA, GPIO_PIN_6};
gpio_pin_typedef_t PA7     = {GPIOA, GPIO_PIN_7};
gpio_pin_typedef_t PA8     = {GPIOA, GPIO_PIN_8};
gpio_pin_typedef_t PA9     = {GPIOA, GPIO_PIN_9};
gpio_pin_typedef_t PA10    = {GPIOA, GPIO_PIN_10};
gpio_pin_typedef_t PA11    = {GPIOA, GPIO_PIN_11};
gpio_pin_typedef_t PA12    = {GPIOA, GPIO_PIN_12};
gpio_pin_typedef_t PA13    = {GPIOA, GPIO_PIN_13};
gpio_pin_typedef_t PA14    = {GPIOA, GPIO_PIN_14};
gpio_pin_typedef_t PA15    = {GPIOA, GPIO_PIN_15};

gpio_pin_typedef_t PB0  = {GPIOB, GPIO_PIN_0};
gpio_pin_typedef_t PB1  = {GPIOB, GPIO_PIN_1};
gpio_pin_typedef_t PB2  = {GPIOB, GPIO_PIN_2};
gpio_pin_typedef_t PB3  = {GPIOB, GPIO_PIN_3};
gpio_pin_typedef_t PB4  = {GPIOB, GPIO_PIN_4};
gpio_pin_typedef_t PB5  = {GPIOB, GPIO_PIN_5};
gpio_pin_typedef_t PB6  = {GPIOB, GPIO_PIN_6};
gpio_pin_typedef_t PB7  = {GPIOB, GPIO_PIN_7};
gpio_pin_typedef_t PB8  = {GPIOB, GPIO_PIN_8};
gpio_pin_typedef_t PB9  = {GPIOB, GPIO_PIN_9};
gpio_pin_typedef_t PB10 = {GPIOB, GPIO_PIN_10};
gpio_pin_typedef_t PB11 = {GPIOB, GPIO_PIN_11};
gpio_pin_typedef_t PB12 = {GPIOB, GPIO_PIN_12};
gpio_pin_typedef_t PB13 = {GPIOB, GPIO_PIN_13};
gpio_pin_typedef_t PB14 = {GPIOB, GPIO_PIN_14};
gpio_pin_typedef_t PB15 = {GPIOB, GPIO_PIN_15};

const char *app_get_gpio_name(gpio_pin_typedef_t gpio)
{
    // 预定义所有可能的 GPIO 名称字符串表
    static const char *const gpio_names[] = {
        "PA0", "PA1", "PA2", "PA3", "PA4", "PA5", "PA6", "PA7",
        "PA8", "PA9", "PA10", "PA11", "PA12", "PA13", "PA14", "PA15",
        "PB0", "PB1", "PB2", "PB3", "PB4", "PB5", "PB6", "PB7",
        "PB8", "PB9", "PB10", "PB11", "PB12", "PB13", "PB14", "PB15"};

    // 快速检查无效 GPIO
    if (!GPIO_IS_VALID(gpio)) {
        return "DEFAULT";
    }

    // 使用位运算快速计算索引
    uint32_t port_offset = (gpio.port == GPIOB) ? 16 : 0;
    uint32_t pin_num     = __builtin_ctz(gpio.pin); // 计算引脚号(0-15)

    // 边界检查
    if (pin_num > 15 || (gpio.port != GPIOA && gpio.port != GPIOB)) {
        return "INVALID";
    }

    return gpio_names[port_offset + pin_num];
}

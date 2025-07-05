#include "gpio.h"
#include <stdint.h>
#include <stdbool.h>
#include "gd32e23x.h"

const char *app_get_gpio_name(gpio_pin_t gpio)
{
    // 快速检查无效GPIO
    if (!GPIO_IS_VALID(gpio)) {
        return "DEF";
    }

    // 预定义所有可能的GPIO名称字符串表
    static const char *const gpio_names[] = {
        "PA0", "PA1", "PA2", "PA3", "PA4", "PA5", "PA6", "PA7",
        "PA8", "PA9", "PA10", "PA11", "PA12", "PA13", "PA14", "PA15",
        "PB0", "PB1", "PB2", "PB3", "PB4", "PB5", "PB6", "PB7",
        "PB8", "PB9", "PB10", "PB11", "PB12", "PB13", "PB14", "PB15"};

    // 计算索引
    uint32_t port_offset = (gpio.port == GPIOB) ? 16 : 0;
    uint32_t pin_num     = __builtin_ctz(gpio.pin); // 使用编译器内置函数计算引脚号(0-15)

    // 边界检查
    if (pin_num > 15 || (gpio.port != GPIOA && gpio.port != GPIOB)) {
        return "INVALID";
    }

    return gpio_names[port_offset + pin_num];
}

bool app_gpio_equal(gpio_pin_t a, gpio_pin_t b)
{
    return (a.port == b.port && a.pin == b.pin);
}

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
        "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7",
        "A8", "A9", "A10", "A11", "A12", "A13", "A14", "A15",
        "B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7",
        "B8", "B9", "B10", "B11", "B12", "B13", "B14", "B15"};

    // 计算索引
    uint32_t port_offset = (gpio.port == GPIOB) ? 16 : 0;
    uint32_t pin_num     = __builtin_ctz(gpio.pin); // 使用编译器内置函数计算引脚号(0-15)

    // 边界检查
    if (pin_num > 15 || (gpio.port != GPIOA && gpio.port != GPIOB)) {
        return "INVALID";
    }

    return gpio_names[port_offset + pin_num];
}

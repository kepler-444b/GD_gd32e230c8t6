#ifndef _PCB_DEVICE_H_
#define _PCB_DEVICE_H_
#include "device_manager.h"

/*
    2025.7.8 舒东升
    新增pcb模块,用于管理所有与硬件相关定义
*/

#if defined PANEL_6KEY_A11
    #define PANEL_VOL_RANGE_DEF          \
        [0] = {.vol_range = {0, 15}},    \
        [1] = {.vol_range = {25, 45}},   \
        [2] = {.vol_range = {85, 110}},  \
        [3] = {.vol_range = {145, 155}}, \
        [4] = {.vol_range = {195, 210}}, \
        [5] = {.vol_range = {230, 255}}
    #define RELAY_GPIO_MAP_DEF {PB12, PB13, PB14, PB15}
    #define LED_W_GPIO_MAP_DEF {PA15, PB3, PB4, PB5, PB6, PB8}

#endif
#if defined PANEL_8KEY_A13
    #define PANEL_VOL_RANGE_DEF         \
        [0] = {.vol_range = {0, 15}},   \
        [1] = {.vol_range = {25, 45}},  \
        [2] = {.vol_range = {85, 110}}, \
        [3] = {.vol_range = {145, 155}}

    #define RELAY_GPIO_MAP_DEF    {PB12, PB13, PB14, PB15} // 继电器 GPIO 映射
    #define LED_W_GPIO_MAP_DEF    {PB0, PB1, PB2, PB10}    // 主面板 LED 白灯 GPIO
    #define LED_W_GPIO_MAP_EX_DEF {PA15, PB3, PB4, PB5}    // 扩展板 LED 白灯 GPIO
    #define LED_Y_GPIO_MAP_DEF    {PA4, PA5, PA6, PA7}     // 主面板 LED 黄灯 GPIO
    #define LED_Y_GPIO_MAP_EX_DEF {PB6, PB7, PB8, PB9}     // 扩展板 LED 黄灯GPIO

#endif

#if defined PANEL_4KEY_A13 || defined PANEL_PLCP_4KEY
    #define PANEL_VOL_RANGE_DEF          \
        [0] = {.vol_range = {25, 40}},   \
        [1] = {.vol_range = {88, 95}},   \
        [2] = {.vol_range = {145, 155}}, \
        [3] = {.vol_range = {0, 10}}

    #define RELAY_GPIO_MAP_DEF {PB12, PB13, PB14, PB15} // 继电器 GPIO 映射
    #define LED_W_GPIO_MAP_DEF {PB3, PB4, PB5, PA15}    // LED 白灯 GPIO
    #define LED_Y_GPIO_MAP_DEF {PB1, PB6, PB7, PB0}     // LED 黄灯 GPIO

#endif

#if defined PANEL_6KEY_A13 || defined PLCP_PANEL_6KEY
    #define PANEL_VOL_RANGE_DEF          \
        [0] = {.vol_range = {30, 40}},   \
        [1] = {.vol_range = {85, 95}},   \
        [2] = {.vol_range = {150, 155}}, \
        [3] = {.vol_range = {0, 5}},     \
        [4] = {.vol_range = {200, 205}}, \
        [5] = {.vol_range = {240, 250}}

    #define RELAY_GPIO_MAP_DEF {PB12, PB13, PB14, PB15}        // 继电器 GPIO 映射
    #define LED_W_GPIO_MAP_DEF {PB3, PB4, PB5, PA15, PA1, PA5} // LED 白灯 GPIO
    #define LED_Y_GPIO_MAP_DEF {PB1, PB6, PB7, PB0, PA6, PA8}  // LED 黄灯 GPIO

#endif

#if defined QUICK_BOX
    #define LED_GPIO_MAP_DEF {PB7, PB6, PB5, PB13, PB14, PB15}
#endif

void panel_gpio_init(void);

void quick_box_gpio_init(void);
#endif

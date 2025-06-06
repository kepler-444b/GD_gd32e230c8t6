#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "../gpio/gpio.h"
#include "../device/device_manager.h"
// 用于panel类型的信息存储
typedef struct
{
    uint8_t key_func;             // 按键功能
    uint8_t key_group;            // 按键分组
    uint8_t key_area;             // 按键区域
    uint8_t key_perm;             // 按键权限
    uint8_t key_scene_group;      // 场景分组
    gpio_pin_typedef_t key_relay; // 按键所控继电器
    gpio_pin_typedef_t key_led;   // 按键所控led
} panel_cfg_t;

bool app_load_config(void);
const panel_cfg_t* app_get_dev_cfg(void);

#endif
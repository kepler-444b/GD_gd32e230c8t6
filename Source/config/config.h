#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "../gpio/gpio.h"
#include "../device/device_manager.h"

/*
    2025.6.1 舒东升
    设备相关的配置,加载串码在此模块中处理,
    硬件相关的(如led relay的GPIO管脚也在此映射)
    由于协议问题,导致 relay的GPIO映射比较复杂
*/

#if defined PANEL_KEY

// 用于panel类型的信息存储
typedef struct
{
    uint8_t func;        // 按键功能
    uint8_t group;       // 双控分组
    uint8_t area;        // 按键区域(高4位:总开关分区,低4位:场景分区)
    uint8_t perm;        // 按键权限
    uint8_t scene_group; // 场景分组

    gpio_pin_t led_w_pin;    // 按键所控白灯
    gpio_pin_t led_y_pin;    // 按键所控黄灯
    gpio_pin_t relay_pin[4]; // 按键所控继电器
} panel_cfg_t;
const panel_cfg_t *app_get_panel_cfg(void);
const panel_cfg_t *app_get_panel_cfg_ex(void);
#endif

#if defined QUICK_BOX
// 用于quick类型的信息存储
typedef struct
{
    uint8_t func;      // 按键类型
    uint8_t group;     // 分组
    uint8_t area;      // 权限
    uint8_t perm;      // 总关/场景区域
    uint8_t key_scene; // 场景
    uint8_t target[6];
} packet;

typedef struct
{
    uint8_t speed;                       // 调光速率
    packet key_packet[LED_NUMBER_COUNT]; // 数据包
} quick_ctg_t;

const quick_ctg_t *app_get_quick_cfg(void);
#endif

bool app_load_config(void);
#endif // _CONFIG_H_
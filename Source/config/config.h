#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "../gpio/gpio.h"
#include "../device/device_manager.h"

#if defined PANEL_KEY
// 用于panel类型的信息存储
typedef struct
{
    uint8_t key_func;                // 按键功能
    uint8_t key_group;               // 按键分组
    uint8_t key_area;                // 按键区域
    uint8_t key_perm;                // 按键权限
    uint8_t key_scene_group;         // 场景分组
    gpio_pin_typedef_t key_relay[4]; // 按键所控继电器
    gpio_pin_typedef_t key_led;      // 按键所控led
} panel_cfg_t;
const panel_cfg_t *app_get_panel_cfg(void);
const panel_cfg_t *app_get_panel_cfg_ex(void);
#endif

#if defined QUICK_BOX
// 用于quick类型的信息存储
typedef struct
{
    uint8_t key_func;  // 按键类型
    uint8_t key_group; // 分组
    uint8_t key_area;  // 权限
    uint8_t key_perm;  // 总关/场景区域
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
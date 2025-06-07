#include "gd32e23x.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include "../flash/flash.h"
#include "../base/base.h"

// 预定义继电器GPIO配置
static const gpio_pin_typedef_t RELAY_GPIO_MAP[] = {PB12, PB13, PB14, PB15};

// 函数声明
void app_panel_get_relay_num(void);
static void app_set_key_relay(panel_cfg_t *panel_cfg, uint8_t relay_val);

static panel_cfg_t my_panel_cfg[KEY_NUMBER_COUNT] = {0};

static uint32_t read_data[32] = {0};
static uint8_t new_data[128]  = {0};
bool app_load_config(void)
{
    if (app_flash_read(CONFIG_START_ADDR, read_data, sizeof(read_data)) != FMC_READY) {
        APP_ERROR("app_flash_read failed\n");
        return false;
    }
    if (app_uint32_to_uint8(read_data, sizeof(read_data) / sizeof(read_data[0]), new_data, sizeof(new_data)) != true) {
        APP_ERROR("app_uint32_to_uint8 error\n");
        return false;
    }

#if defined PANEL_KEY // panel 类型

    if (KEY_NUMBER_COUNT == 6) {
        for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
            if (i < 4) {
                my_panel_cfg[i].key_func        = new_data[i + 1];
                my_panel_cfg[i].key_group       = new_data[i + 5];
                my_panel_cfg[i].key_area        = new_data[i + 9];
                my_panel_cfg[i].key_perm        = new_data[i + 13];
                my_panel_cfg[i].key_scene_group = new_data[i + 17];
            } else if (i == 4) {
                my_panel_cfg[i].key_func        = new_data[21];
                my_panel_cfg[i].key_group       = new_data[22];
                my_panel_cfg[i].key_area        = new_data[23];
                my_panel_cfg[i].key_perm        = new_data[26];
                my_panel_cfg[i].key_scene_group = new_data[27];
            } else if (i == 5) {
                my_panel_cfg[i].key_func        = new_data[28];
                my_panel_cfg[i].key_group       = new_data[29];
                my_panel_cfg[i].key_area        = new_data[30];
                my_panel_cfg[i].key_perm        = new_data[31];
                my_panel_cfg[i].key_scene_group = new_data[32];
            }
        }

        my_panel_cfg[0].key_led = PA15;
        my_panel_cfg[1].key_led = PB3;
        my_panel_cfg[2].key_led = PB4;
        my_panel_cfg[3].key_led = PB5;
        my_panel_cfg[4].key_led = PB6;
        my_panel_cfg[5].key_led = PB8;
        app_panel_get_relay_num();

        for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
            APP_PRINTF("[%d] ", i);
            APP_PRINTF("[%02X] ", my_panel_cfg[i].key_func);
            APP_PRINTF("[%02X] ", my_panel_cfg[i].key_group);
            APP_PRINTF("[%02X] ", my_panel_cfg[i].key_area);
            APP_PRINTF("[%02X] ", my_panel_cfg[i].key_perm);
            APP_PRINTF("[%s] ", app_get_gpio_name(my_panel_cfg[i].key_led));
            for (uint8_t j = 0; j < 4; j++) {
                APP_PRINTF("[%s] ", app_get_gpio_name(my_panel_cfg[i].key_relay[j]));
            }
            APP_PRINTF("\n");
        }
    }
#endif
    return true;
}

void app_panel_get_relay_num(void)
{
    const uint8_t base_offset = 34; // 偏移到(34,35,36 用于按键 12,34,56 所控制的继电器)
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t byte_offset = base_offset + (i / 2); // 提取半字节

        // 奇数取字节的高4位,偶数取字节的低4位
        uint8_t relay_num = (i % 2) ? (new_data[byte_offset] >> 4) : (new_data[byte_offset] & 0x0F);
        app_set_key_relay(&my_panel_cfg[i], relay_num);
    }
}
static void app_set_key_relay(panel_cfg_t *panel_cfg, uint8_t relay_num)
{
    memset(panel_cfg->key_relay, 0, sizeof(panel_cfg->key_relay));
    for (uint8_t i = 0; i < 4; i++) { // 只有4个继电器
        panel_cfg->key_relay[i] = (relay_num & (1 << i)) ? RELAY_GPIO_MAP[i] : DEFAULT;
    }
}
const panel_cfg_t *app_get_dev_cfg(void)
{
    return my_panel_cfg;
}

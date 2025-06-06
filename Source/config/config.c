#include "gd32e23x.h"
#include "config.h"
#include <stdio.h>
#include "../flash/flash.h"
#include "../base/base.h"

gpio_pin_typedef_t app_panel_get_relay_num(uint8_t key_num, uint8_t *data);
static panel_cfg_t my_panel_cfg[KEY_NUMBER_COUNT] = {0};

bool app_load_config(void)
{
    uint32_t read_data[32] = {0};
    if (app_flash_read(CONFIG_START_ADDR, read_data, sizeof(read_data)) != FMC_READY) {
        APP_ERROR("app_flash_read failed\n");
        return false;
    }
    uint8_t new_data[128] = {0};
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
            my_panel_cfg[i].key_relay = app_panel_get_relay_num(i, new_data);
        }

        my_panel_cfg[0].key_led = PA15;
        my_panel_cfg[1].key_led = PB3;
        my_panel_cfg[2].key_led = PB4;
        my_panel_cfg[3].key_led = PB5;
        my_panel_cfg[4].key_led = PB6;
        my_panel_cfg[5].key_led = PB8;
        for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
            APP_PRINTF("[%02X] ", my_panel_cfg[i].key_func);
            APP_PRINTF("[%02X] ", my_panel_cfg[i].key_group);
            APP_PRINTF("[%02X] ", my_panel_cfg[i].key_area);
            APP_PRINTF("[%02X] ", my_panel_cfg[i].key_perm);
            APP_PRINTF("[%s] ", app_get_gpio_name(my_panel_cfg[i].key_relay));
            APP_PRINTF("[%s] ", app_get_gpio_name(my_panel_cfg[i].key_led));
            APP_PRINTF("\n");
        }
    }
#endif
    return true;
}

gpio_pin_typedef_t app_panel_get_relay_num(uint8_t key_num, uint8_t *data)
{
    uint8_t value;
    switch (key_num) {
        case 0:
            value = data[34] & 0x0F;
            break;
        case 1:
            value = data[34] >> 4;
            break;
        case 2:
            value = data[35] & 0x0F;
            break;
        case 3:
            value = data[35] >> 4;
            break;
        case 4:
            value = data[36] & 0x0F;
            break;
        case 5:
            value = data[36] >> 4;
            break;
        default:
            return DEFAULT;
    }

    switch (value) {
        case 1:
            return PB12;
        case 2:
            return PB13;
        case 4:
            return PB14;
        case 8:
            return PB15;
        default:
            return DEFAULT;
    }
}

const panel_cfg_t *app_get_dev_cfg(void)
{
    return my_panel_cfg;
}

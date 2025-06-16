#include "gd32e23x.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include "../flash/flash.h"
#include "../base/base.h"
#include "../device/device_manager.h"

#if defined PANEL_KEY

#if defined PANEL_6KEY
static panel_cfg_t my_panel_cfg[KEY_NUMBER_COUNT] = {0};

static const gpio_pin_typedef_t RELAY_GPIO_MAP[KEY_NUMBER_COUNT] = {PB12, PB13, PB14, PB15};
static const gpio_pin_typedef_t LED_W_GPIO_MAP[KEY_NUMBER_COUNT] = {PA15, PB3, PB4, PB5, PB6, PB8};
#endif

#if defined PANEL_8KEY
static panel_cfg_t my_panel_cfg[KEY_NUMBER_COUNT]    = {0};
static panel_cfg_t my_panel_cfg_ex[KEY_NUMBER_COUNT] = {0};

// 预定义 GPIO
static const gpio_pin_typedef_t RELAY_GPIO_MAP[] = {PB12, PB13, PB14, PB15};

static const gpio_pin_typedef_t LED_W_GPIO_MAP[KEY_NUMBER_COUNT]    = {PB0, PB1, PB2, PB10};
static const gpio_pin_typedef_t LED_W_GPIO_MAP_EX[KEY_NUMBER_COUNT] = {PA15, PB3, PB4, PB5};

static const gpio_pin_typedef_t LED_Y_GPIO_MAP[KEY_NUMBER_COUNT]    = {PA4, PA5, PA6, PA7};
static const gpio_pin_typedef_t LED_Y_GPIO_MAP_EX[KEY_NUMBER_COUNT] = {PB6, PB7, PB8, PB9};
#endif

// 函数声明
void app_panel_get_relay_num(void);
static void app_set_relay_pin(panel_cfg_t *panel_cfg, uint8_t relay_val);
#endif

#if defined QUICK_BOX
static quick_ctg_t quick_cfg;                  // 分配内存
static quick_ctg_t *my_quick_cfg = &quick_cfg; // 指针指向已分配的结构体
#endif

static uint32_t read_data[24] = {0};
static uint8_t new_data[96]   = {0};

#if defined PANEL_8KEY
static uint32_t read_data_ex[24] = {0};
static uint8_t new_data_ex[96]   = {0};
#endif

bool app_load_config(void)
{
    // app_flash_mass_erase();  // 擦除整个扇区(测试使用)
    __disable_irq();
    if (app_flash_read(CONFIG_START_ADDR, read_data, sizeof(read_data)) != FMC_READY) {
        APP_ERROR("app_flash_read failed\n");
        return false;
    }
    if (app_uint32_to_uint8(read_data, sizeof(read_data) / sizeof(read_data[0]), new_data, sizeof(new_data)) != true) {
        APP_ERROR("app_uint32_to_uint8 error\n");
        return false;
    }
#if defined PANEL_8KEY

    if (app_flash_read(CONFIG_EXTEN_ADDR, read_data_ex, sizeof(read_data_ex)) != FMC_READY) {
        APP_ERROR("app_flash_read failed\n");
        return false;
    }
    if (app_uint32_to_uint8(read_data_ex, sizeof(read_data_ex) / sizeof(read_data_ex[0]), new_data_ex, sizeof(new_data_ex)) != true) {
        APP_ERROR("app_uint32_to_uint8 error\n");
        return false;
    }
    __enable_irq();
#endif

#if defined PANEL_KEY // 填充 my_panel_cfg 结构体

#if defined PANEL_6KEY
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
        my_panel_cfg[i].led_w_pin = LED_W_GPIO_MAP[i];
    }
    app_panel_get_relay_num();

    for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
        APP_PRINTF("[%d] ", i);
        APP_PRINTF("[%02X] ", my_panel_cfg[i].key_func);
        APP_PRINTF("[%02X] ", my_panel_cfg[i].key_group);
        APP_PRINTF("[%02X] ", my_panel_cfg[i].key_area);
        APP_PRINTF("[%02X] ", my_panel_cfg[i].key_perm);
        APP_PRINTF("[%s] ", app_get_gpio_name(my_panel_cfg[i].led_w_pin));
        for (uint8_t j = 0; j < 4; j++) {
            APP_PRINTF("[%s] ", app_get_gpio_name(my_panel_cfg[i].relay_pin[j]));
        }
        APP_PRINTF("\n");
    }

#endif

#if defined PANEL_8KEY
    for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
        my_panel_cfg[i].key_func        = new_data[i + 1];
        my_panel_cfg[i].key_group       = new_data[i + 5];
        my_panel_cfg[i].key_area        = new_data[i + 9];
        my_panel_cfg[i].key_perm        = new_data[i + 13];
        my_panel_cfg[i].key_scene_group = new_data[i + 17];
        my_panel_cfg[i].led_w_pin       = LED_W_GPIO_MAP[i];
        my_panel_cfg[i].led_y_pin       = LED_Y_GPIO_MAP[i];

        my_panel_cfg_ex[i].key_func        = new_data_ex[i + 1];
        my_panel_cfg_ex[i].key_group       = new_data_ex[i + 5];
        my_panel_cfg_ex[i].key_area        = new_data_ex[i + 9];
        my_panel_cfg_ex[i].key_perm        = new_data_ex[i + 13];
        my_panel_cfg_ex[i].key_scene_group = new_data_ex[i + 17];
        my_panel_cfg_ex[i].led_w_pin       = LED_W_GPIO_MAP_EX[i];
        my_panel_cfg_ex[i].led_y_pin       = LED_Y_GPIO_MAP_EX[i];
    }

    app_panel_get_relay_num();

    for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
        APP_PRINTF("[%d] ", i);
        APP_PRINTF("[%02X] ", my_panel_cfg_ex[i].key_func);
        APP_PRINTF("[%02X] ", my_panel_cfg_ex[i].key_group);
        APP_PRINTF("[%02X] ", my_panel_cfg_ex[i].key_area);
        APP_PRINTF("[%02X] ", my_panel_cfg_ex[i].key_perm);
        APP_PRINTF("[%s] ", app_get_gpio_name(my_panel_cfg_ex[i].led_w_pin));
        for (uint8_t j = 0; j < 4; j++) {
            APP_PRINTF("[%s] ", app_get_gpio_name(my_panel_cfg_ex[i].relay_pin[j]));
        }
        APP_PRINTF("\n");
    }
#endif

#endif

#if defined QUICK_BOX // 填充 my_quick_cfg 结构体

    uint8_t packet_count = new_data[1];
    my_quick_cfg->speed  = new_data[2];
    for (uint8_t i = 0; i < packet_count; i++) {
        memcpy(&my_quick_cfg->key_packet[i], &new_data[3 + (i * 11)], sizeof(packet));

        APP_PRINTF("%02X %02X %02X %02X %02X  ", my_quick_cfg->key_packet[i].key_func,
                   my_quick_cfg->key_packet[i].key_group,
                   my_quick_cfg->key_packet[i].key_area,
                   my_quick_cfg->key_packet[i].key_perm,
                   my_quick_cfg->key_packet[i].key_scene);
        for (uint8_t j = 0; j < 6; j++) {
            APP_PRINTF("%02X ", my_quick_cfg->key_packet[i].target[j]);
        }
        APP_PRINTF("\n");
    }

#endif
    return true;
}

#if defined PANEL_KEY
void app_panel_get_relay_num(void)
{
    const uint8_t base_offset = 34; // 偏移到(34,35,36 用于按键 12,34,56 所控制的继电器)
    for (uint8_t i = 0; i < KEY_NUMBER_COUNT; i++) {
        uint8_t byte_offset = base_offset + (i / 2); // 提取半字节

        // 奇数取字节的高4位,偶数取字节的低4位
        uint8_t relay_num = (i % 2) ? (new_data[byte_offset] >> 4) : (new_data[byte_offset] & 0x0F);
        app_set_relay_pin(&my_panel_cfg[i], relay_num);
#if defined PANEL_8KEY
        uint8_t relay_num_ex = (i % 2) ? (new_data_ex[byte_offset] >> 4) : (new_data_ex[byte_offset] & 0x0F);
        app_set_relay_pin(&my_panel_cfg_ex[i], relay_num_ex);
#endif
    }
}

static void app_set_relay_pin(panel_cfg_t *panel_cfg, uint8_t relay_num)
{
    memset(panel_cfg->relay_pin, 0, sizeof(panel_cfg->relay_pin));
    for (uint8_t i = 0; i < 4; i++) { // 只有4个继电器
        panel_cfg->relay_pin[i] = (relay_num & (1 << i)) ? RELAY_GPIO_MAP[i] : DEFAULT;
    }
}

#endif

#if defined PANEL_KEY
const panel_cfg_t *app_get_panel_cfg(void)
{
    return my_panel_cfg;
}
#if defined PANEL_8KEY
const panel_cfg_t *app_get_panel_cfg_ex(void)
{
    return my_panel_cfg_ex;
}
#endif
#endif

#if defined QUICK_BOX
const quick_ctg_t *app_get_quick_cfg(void)
{
    return my_quick_cfg;
}
#endif

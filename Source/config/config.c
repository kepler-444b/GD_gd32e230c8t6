#include "gd32e23x.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include "../flash/flash.h"
#include "../base/base.h"
#include "../device/device_manager.h"

#if defined PANEL_KEY

#if defined PANEL_6KEY
static panel_cfg_t my_panel_cfg[KEY_NUMBER] = {0};

static const gpio_pin_t RELAY_GPIO_MAP[KEY_NUMBER] = {PB12, PB13, PB14, PB15};
static const gpio_pin_t LED_W_GPIO_MAP[KEY_NUMBER] = {PA15, PB3, PB4, PB5, PB6, PB8};
#endif

#if defined PANEL_8KEY
static panel_cfg_t my_panel_cfg[KEY_NUMBER]    = {0};
static panel_cfg_t my_panel_cfg_ex[KEY_NUMBER] = {0};

// 预定义 GPIO
static const gpio_pin_t RELAY_GPIO_MAP[] = {PB12, PB13, PB14, PB15};

static const gpio_pin_t LED_W_GPIO_MAP[KEY_NUMBER]    = {PB0, PB1, PB2, PB10};
static const gpio_pin_t LED_W_GPIO_MAP_EX[KEY_NUMBER] = {PA15, PB3, PB4, PB5};

static const gpio_pin_t LED_Y_GPIO_MAP[KEY_NUMBER]    = {PA4, PA5, PA6, PA7};
static const gpio_pin_t LED_Y_GPIO_MAP_EX[KEY_NUMBER] = {PB6, PB7, PB8, PB9};
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
    memset(read_data, 0, sizeof(read_data));
    memset(new_data, 0, sizeof(new_data));
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

    memset(read_data_ex, 0, sizeof(read_data_ex));
    memset(new_data_ex, 0, sizeof(new_data_ex));
    if (app_flash_read(CONFIG_EXTEN_ADDR, read_data_ex, sizeof(read_data_ex)) != FMC_READY) {
        APP_ERROR("app_flash_read failed\n");
        return false;
    }
    if (app_uint32_to_uint8(read_data_ex, sizeof(read_data_ex) / sizeof(read_data_ex[0]), new_data_ex, sizeof(new_data_ex)) != true) {
        APP_ERROR("app_uint32_to_uint8 error\n");
        return false;
    }
#endif
    __enable_irq();

#if defined PANEL_KEY // 填充 my_panel_cfg 结构体

#if defined PANEL_6KEY
    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
        panel_cfg_t *const p_cfg = &my_panel_cfg[i];
        if (i < 4) {
            p_cfg->func        = new_data[i + 1];
            p_cfg->group       = new_data[i + 5];
            p_cfg->area        = new_data[i + 9];
            p_cfg->perm        = new_data[i + 13];
            p_cfg->scene_group = new_data[i + 17];
        } else if (i == 4) {
            p_cfg->func        = new_data[21];
            p_cfg->group       = new_data[22];
            p_cfg->area        = new_data[23];
            p_cfg->perm        = new_data[26];
            p_cfg->scene_group = new_data[27];
        } else if (i == 5) {
            p_cfg->func        = new_data[28];
            p_cfg->group       = new_data[29];
            p_cfg->area        = new_data[30];
            p_cfg->perm        = new_data[31];
            p_cfg->scene_group = new_data[32];
        }
        p_cfg->led_w_pin = LED_W_GPIO_MAP[i];
    }
    app_panel_get_relay_num();

    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
        panel_cfg_t *const p_cfg = &my_panel_cfg[i];

        APP_PRINTF("%02X %02X %02X %02X ", p_cfg->func, p_cfg->group, p_cfg->area, p_cfg->perm);
        const char *led_w_name = app_get_gpio_name(p_cfg->led_w_pin);
        APP_PRINTF("%-4s| ", led_w_name);
        for (uint8_t j = 0; j < 4; j++) {
            const char *relay_name = app_get_gpio_name(p_cfg->relay_pin[j]);
            APP_PRINTF("%-4s ", relay_name);
        }
        APP_PRINTF("\n");
    }

#endif // PANEL_6KEY

#if defined PANEL_8KEY
    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
        panel_cfg_t *const p_cfg    = &my_panel_cfg[i];
        panel_cfg_t *const p_cfg_ex = &my_panel_cfg_ex[i];

        p_cfg->func        = new_data[i + 1];
        p_cfg->group       = new_data[i + 5];
        p_cfg->area        = new_data[i + 9];
        p_cfg->perm        = new_data[i + 13];
        p_cfg->scene_group = new_data[i + 17];
        p_cfg->led_w_pin   = LED_W_GPIO_MAP[i];
        p_cfg->led_y_pin   = LED_Y_GPIO_MAP[i];

        p_cfg_ex->func        = new_data_ex[i + 1];
        p_cfg_ex->group       = new_data_ex[i + 5];
        p_cfg_ex->area        = new_data_ex[i + 9];
        p_cfg_ex->perm        = new_data_ex[i + 13];
        p_cfg_ex->scene_group = new_data_ex[i + 17];
        p_cfg_ex->led_w_pin   = LED_W_GPIO_MAP_EX[i];
        p_cfg_ex->led_y_pin   = LED_Y_GPIO_MAP_EX[i];
    }
    app_panel_get_relay_num();

    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
        panel_cfg_t *const p_cfg = &my_panel_cfg[i];

        APP_PRINTF("%02X %02X %02X %02X ", p_cfg->func, p_cfg->group, p_cfg->area, p_cfg->perm);
        const char *led_w_name = app_get_gpio_name(p_cfg->led_w_pin);
        const char *led_y_name = app_get_gpio_name(p_cfg->led_y_pin);
        APP_PRINTF("%-4s %-4s| ", led_w_name, led_y_name);
        for (uint8_t j = 0; j < 4; j++) {
            const char *relay_name = app_get_gpio_name(p_cfg->relay_pin[j]);
            APP_PRINTF("%-4s ", relay_name);
        }
        APP_PRINTF("\n");
    }
    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
        panel_cfg_t *const p_cfg_ex = &my_panel_cfg_ex[i];

        APP_PRINTF("%02X %02X %02X %02X ", p_cfg_ex->func, p_cfg_ex->group, p_cfg_ex->area, p_cfg_ex->perm);
        const char *led_w_name = app_get_gpio_name(p_cfg_ex->led_w_pin);
        const char *led_y_name = app_get_gpio_name(p_cfg_ex->led_y_pin);
        APP_PRINTF("%-4s %-4s| ", led_w_name, led_y_name);
        for (uint8_t j = 0; j < 4; j++) {
            const char *relay_name = app_get_gpio_name(p_cfg_ex->relay_pin[j]);
            APP_PRINTF("%-4s ", relay_name);
        }
        APP_PRINTF("\n");
    }
#endif // PANEL_8KEY
#endif // PANEL_KEY

#if defined QUICK_BOX // 填充 my_quick_cfg 结构体

    uint8_t packet_count = new_data[1];
    my_quick_cfg->speed  = new_data[2];
    for (uint8_t i = 0; i < packet_count; i++) {
        memcpy(&my_quick_cfg->key_packet[i], &new_data[3 + (i * 11)], sizeof(packet));

        APP_PRINTF("%02X %02X %02X %02X %02X  ", my_quick_cfg->key_packet[i].func,
                   my_quick_cfg->key_packet[i].group,
                   my_quick_cfg->key_packet[i].area,
                   my_quick_cfg->key_packet[i].perm,
                   my_quick_cfg->key_packet[i].key_scene);
        for (uint8_t j = 0; j < 6; j++) {
            APP_PRINTF("%02X ", my_quick_cfg->key_packet[i].target[j]);
        }
        APP_PRINTF("\n");
    }

#endif // QUICK_BOX
    return true;
}

#if defined PANEL_KEY // 辅助函数(将relay的pin映射到key)
void app_panel_get_relay_num(void)
{
    const uint8_t base_offset = 34; // 按键1所控继电器由34的第四位定义,按键2所控继电器由34的高4位定义,即34,35,36用于按键12,34,56
    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
        uint8_t byte_offset = base_offset + (i / 2); // 通过整数除法实现案件配对(每2个按键共用一个字节)

        // 奇数按键取字节的高4位,偶数按键取字节的低4位
        uint8_t relay_num = (i % 2) ? H_BIT(new_data[byte_offset]) : L_BIT(new_data[byte_offset]);
        app_set_relay_pin(&my_panel_cfg[i], relay_num);
#if defined PANEL_8KEY
        uint8_t relay_num_ex = (i % 2) ? H_BIT(new_data_ex[byte_offset]) : L_BIT(new_data_ex[byte_offset]);
        app_set_relay_pin(&my_panel_cfg_ex[i], relay_num_ex);
#endif
    }
}

static void app_set_relay_pin(panel_cfg_t *panel_cfg, uint8_t relay_num)
{
    panel_cfg->relay_pin[0] = BIT0(relay_num) ? RELAY_GPIO_MAP[0] : DEF;
    panel_cfg->relay_pin[1] = BIT1(relay_num) ? RELAY_GPIO_MAP[1] : DEF;
    panel_cfg->relay_pin[2] = BIT2(relay_num) ? RELAY_GPIO_MAP[2] : DEF;
    panel_cfg->relay_pin[3] = BIT3(relay_num) ? RELAY_GPIO_MAP[3] : DEF;
}
#endif // PANEL_KEY

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

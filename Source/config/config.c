#include "gd32e23x.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include "../flash/flash.h"
#include "../base/base.h"
#include "../device/device_manager.h"
#include "../device/pcb_device.h"

#if defined PANEL_KEY || defined PANEL_PLCP
static panel_cfg_t my_panel_cfg[KEY_NUMBER] = {0};
    #if defined PANEL_8KEY_A13
static panel_cfg_t my_panel_cfg_ex[KEY_NUMBER] = {0};
    #endif
#endif
#if defined QUICK_BOX
static quick_ctg_t quick_cfg[LED_NUMBER] = {0};
#endif
// 函数声明
static void app_load_quick_box(uint8_t *data);
static void app_load_panel_4key(uint8_t *data);
static void app_load_panel_6key(uint8_t *data);
static void app_load_panel_8key(uint8_t *data, uint8_t *data_ex);
static void app_panel_get_relay_num(uint8_t *data, const gpio_pin_t *relay_map, bool is_ex);

void app_load_config(void)
{
    // app_flash_mass_erase();  // 擦除整个扇区(测试使用)
    static uint32_t read_data[32] = {0};
    static uint8_t new_data[128]  = {0};
    memset(read_data, 0, sizeof(read_data));
    memset(new_data, 0, sizeof(new_data));
    __disable_irq();
    if (app_flash_read(CONFIG_START_ADDR, read_data, sizeof(read_data)) != FMC_READY) {
        APP_ERROR("app_flash_read failed\n");
    }
    if (app_uint32_to_uint8(read_data, sizeof(read_data) / sizeof(read_data[0]), new_data, sizeof(new_data)) != true) {
        APP_ERROR("app_uint32_to_uint8 error\n");
    }
#if defined PANEL_8KEY_A13

    static uint32_t read_data_ex[24] = {0};
    static uint8_t new_data_ex[96]   = {0};
    memset(read_data_ex, 0, sizeof(read_data_ex));
    memset(new_data_ex, 0, sizeof(new_data_ex));
    if (app_flash_read(CONFIG_EXTEN_ADDR, read_data_ex, sizeof(read_data_ex)) != FMC_READY) {
        APP_ERROR("app_flash_read failed\n");
    }
    if (app_uint32_to_uint8(read_data_ex, sizeof(read_data_ex) / sizeof(read_data_ex[0]), new_data_ex, sizeof(new_data_ex)) != true) {
        APP_ERROR("app_uint32_to_uint8 error\n");
    }
#endif
    __enable_irq();

#if defined PANEL_KEY
    #if defined PANEL_6KEY_A11 || defined PANEL_6KEY_A13
    app_load_panel_6key(new_data);
    #endif
    #if defined PANEL_8KEY_A13
    app_load_panel_8key(new_data, new_data_ex);
    #endif
    #if defined PANEL_4KEY_A13
    app_load_panel_4key(new_data);
    #endif
#endif
#if defined QUICK_BOX
    app_load_quick_box(new_data);
#endif
}

void app_plcp_map(void)
{
#if defined PANEL_PLCP_4KEY || defined PLCP_PANEL_6KEY
    static const gpio_pin_t RELAY_GPIO_MAP[RELAY_NUMBER] = RELAY_GPIO_MAP_DEF;
    static const gpio_pin_t LED_W_GPIO_MAP[KEY_NUMBER]   = LED_W_GPIO_MAP_DEF;
    static const gpio_pin_t LED_Y_GPIO_MAP[KEY_NUMBER]   = LED_Y_GPIO_MAP_DEF;

    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
        panel_cfg_t *const p_cfg = &my_panel_cfg[i];

        p_cfg->led_w_pin    = LED_W_GPIO_MAP[i];
        p_cfg->led_y_pin    = LED_Y_GPIO_MAP[i];
        p_cfg->relay_pin[0] = RELAY_GPIO_MAP[i];
    }
#endif
}

static void app_load_panel_4key(uint8_t *data)
{
#if defined PANEL_4KEY_A13
    static const gpio_pin_t RELAY_GPIO_MAP[RELAY_NUMBER] = RELAY_GPIO_MAP_DEF;
    static const gpio_pin_t LED_W_GPIO_MAP[KEY_NUMBER]   = LED_W_GPIO_MAP_DEF;
    static const gpio_pin_t LED_Y_GPIO_MAP[KEY_NUMBER]   = LED_Y_GPIO_MAP_DEF;
    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
        panel_cfg_t *const p_cfg = &my_panel_cfg[i];
        p_cfg->func              = data[i + 1];
        p_cfg->group             = data[i + 5];
        p_cfg->area              = data[i + 9];
        p_cfg->perm              = data[i + 13];
        p_cfg->scene_group       = data[i + 17];
        p_cfg->led_w_pin         = LED_W_GPIO_MAP[i];
        p_cfg->led_y_pin         = LED_Y_GPIO_MAP[i];
    }
    app_panel_get_relay_num(data, RELAY_GPIO_MAP, false);
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
    APP_PRINTF("\n");
#endif
}
static void app_load_panel_6key(uint8_t *data)
{
#if defined PANEL_6KEY_A13
    static const gpio_pin_t RELAY_GPIO_MAP[RELAY_NUMBER] = RELAY_GPIO_MAP_DEF;
    static const gpio_pin_t LED_W_GPIO_MAP[KEY_NUMBER]   = LED_W_GPIO_MAP_DEF;
    static const gpio_pin_t LED_Y_GPIO_MAP[KEY_NUMBER]   = LED_Y_GPIO_MAP_DEF;
    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
        panel_cfg_t *const p_cfg = &my_panel_cfg[i];
        if (i < 4) {
            p_cfg->func        = data[i + 1];
            p_cfg->group       = data[i + 5];
            p_cfg->area        = data[i + 9];
            p_cfg->perm        = data[i + 13];
            p_cfg->scene_group = data[i + 17];
        } else if (i == 4) {
            p_cfg->func        = data[21];
            p_cfg->group       = data[22];
            p_cfg->area        = data[23];
            p_cfg->perm        = data[26];
            p_cfg->scene_group = data[27];
        } else if (i == 5) {
            p_cfg->func        = data[28];
            p_cfg->group       = data[29];
            p_cfg->area        = data[30];
            p_cfg->perm        = data[31];
            p_cfg->scene_group = data[32];
        }
        p_cfg->led_w_pin = LED_W_GPIO_MAP[i];
        p_cfg->led_y_pin = LED_Y_GPIO_MAP[i];
    }
    app_panel_get_relay_num(data, RELAY_GPIO_MAP, false);

    // 打印配置信息
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
#endif /* PANEL_6KEY_A11 */
}

static void app_load_panel_8key(uint8_t *data, uint8_t *data_ex)
{
#if defined PANEL_8KEY_A13
    static const gpio_pin_t RELAY_GPIO_MAP[RELAY_NUMBER]  = RELAY_GPIO_MAP_DEF;
    static const gpio_pin_t LED_W_GPIO_MAP[KEY_NUMBER]    = LED_W_GPIO_MAP_DEF;
    static const gpio_pin_t LED_W_GPIO_MAP_EX[KEY_NUMBER] = LED_W_GPIO_MAP_EX_DEF;
    static const gpio_pin_t LED_Y_GPIO_MAP[KEY_NUMBER]    = LED_Y_GPIO_MAP_DEF;
    static const gpio_pin_t LED_Y_GPIO_MAP_EX[KEY_NUMBER] = LED_Y_GPIO_MAP_EX_DEF;

    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
        panel_cfg_t *const p_cfg    = &my_panel_cfg[i];
        panel_cfg_t *const p_cfg_ex = &my_panel_cfg_ex[i];

        p_cfg->func        = data[i + 1];
        p_cfg->group       = data[i + 5];
        p_cfg->area        = data[i + 9];
        p_cfg->perm        = data[i + 13];
        p_cfg->scene_group = data[i + 17];
        p_cfg->led_w_pin   = LED_W_GPIO_MAP[i];
        p_cfg->led_y_pin   = LED_Y_GPIO_MAP[i];

        p_cfg_ex->func        = data_ex[i + 1];
        p_cfg_ex->group       = data_ex[i + 5];
        p_cfg_ex->area        = data_ex[i + 9];
        p_cfg_ex->perm        = data_ex[i + 13];
        p_cfg_ex->scene_group = data_ex[i + 17];
        p_cfg_ex->led_w_pin   = LED_W_GPIO_MAP_EX[i];
        p_cfg_ex->led_y_pin   = LED_Y_GPIO_MAP_EX[i];
    }
    app_panel_get_relay_num(data, RELAY_GPIO_MAP, false);
    app_panel_get_relay_num(data_ex, RELAY_GPIO_MAP, true);

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
    APP_PRINTF("\n");
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
#endif // PANEL_8KEY_A13
}

static void app_load_quick_box(uint8_t *data)
{
#if defined QUICK_BOX

    // 对于快装盒子,最多支持8路灯,前3路是PWM,后3路是继电器,最后2路保留(顺序依次为 LED1,LED2,LED3,K1,K2,K3)
    static const gpio_pin_t LED_GPIO_MAP[LED_NUMBER] = LED_GPIO_MAP_DEF;
    for (uint8_t i = 0; i < LED_NUMBER; i++) {
        quick_ctg_t *const p_cfg = &quick_cfg[i];
        memcpy(p_cfg, &data[i * 14], 14);
        p_cfg->led_pin = LED_GPIO_MAP[i];
    }
    for (uint8_t i = 0; i < LED_NUMBER; i++) {
        quick_ctg_t *const p_cfg = &quick_cfg[i];
        APP_PRINTF("%02X %02X %02X %02X %02X %02X ", p_cfg->func, p_cfg->group, p_cfg->perm, p_cfg->area, p_cfg->scene_group, p_cfg->lum);
        const char *led_name = app_get_gpio_name(p_cfg->led_pin); // led 管脚
        APP_PRINTF("%-4s | ", led_name);
        for (uint8_t j = 0; j < LED_NUMBER; j++) {
            APP_PRINTF("%02X ", p_cfg->scene_lun[j]);
        }
        APP_PRINTF("\n");
    }
#endif /* QUICK_BOX */
}

// 辅助函数(将 relay 的 pin 映射到 key)
static void app_panel_get_relay_num(uint8_t *data, const gpio_pin_t *relay_map, bool is_ex)
{
#if defined PANEL_KEY
    const uint8_t base_offset = 34;

    for (uint8_t i = 0; i < KEY_NUMBER; i++) {
        uint8_t byte_offset = base_offset + (i / 2);
        uint8_t relay_num   = (i % 2) ? H_BIT(data[byte_offset]) : L_BIT(data[byte_offset]);

        if (!is_ex) {
            my_panel_cfg[i].relay_pin[0] = BIT0(relay_num) ? relay_map[0] : DEF;
            my_panel_cfg[i].relay_pin[1] = BIT1(relay_num) ? relay_map[1] : DEF;
            my_panel_cfg[i].relay_pin[2] = BIT2(relay_num) ? relay_map[2] : DEF;
            my_panel_cfg[i].relay_pin[3] = BIT3(relay_num) ? relay_map[3] : DEF;
        }

    #if defined PANEL_8KEY_A13
        else if (is_ex) {
            my_panel_cfg_ex[i].relay_pin[0] = BIT0(relay_num) ? relay_map[0] : DEF;
            my_panel_cfg_ex[i].relay_pin[1] = BIT1(relay_num) ? relay_map[1] : DEF;
            my_panel_cfg_ex[i].relay_pin[2] = BIT2(relay_num) ? relay_map[2] : DEF;
            my_panel_cfg_ex[i].relay_pin[3] = BIT3(relay_num) ? relay_map[3] : DEF;
        }
    #endif
    }
#endif
}

#if defined PANEL_KEY || defined PANEL_PLCP
const panel_cfg_t *app_get_panel_cfg(void)
{
    return my_panel_cfg;
}
    #if defined PANEL_8KEY_A13
const panel_cfg_t *app_get_panel_cfg_ex(void)
{
    return my_panel_cfg_ex;
}
    #endif
#endif

#if defined QUICK_BOX
const quick_ctg_t *app_get_quick_cfg(void)
{
    return quick_cfg;
}
#endif

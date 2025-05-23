#include "gd32e23x.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "protocol.h"
#include "../uart/uart.h"
#include "../base/debug.h"
#include "../flash/flash.h"
#include "../timer/timer.h"
#include "../base/base.h"
#include "../eventbus/eventbus.h"

static valid_data_t my_valid_data = {0};

bool enter_config = false;

// 函数声明
void app_proto_check(uint8_t *data, uint16_t length);
void app_save_config(valid_data_t *boj);
uint8_t calcrc_data(uint8_t *rxbuf, uint8_t len);

static ValidDataCallback valid_data_callback = NULL;
void app_proto_init(void)
{
    app_usart0_rx_callback(app_proto_check);
}

void app_valid_data_callback(ValidDataCallback callback)
{
    valid_data_callback = callback;
}
void app_proto_check(uint8_t *data, uint16_t length)
{
    APP_PRINTF("[RECV]:%s\n", data);
    // 检查协议头
    if (strncmp((char *)data, "+RECV:", 6) != 0) {
        return;
    }

    // 查找有效数据边界
    uint8_t *start_ptr = (uint8_t *)strchr((char *)data, '"');
    if (start_ptr == NULL) {
        return;
    }

    uint8_t *end_ptr = (uint8_t *)strchr((char *)(start_ptr + 1), '"');
    if (end_ptr == NULL) {
        return;
    }

    // 提取有效数据
    my_valid_data.length = (end_ptr - start_ptr - 1) / 2;
    for (uint16_t i = 0; i < my_valid_data.length; i++) {
        my_valid_data.data[i] = HEX_TO_BYTE(start_ptr + 1 + i * 2);
    }

    // 根据命令类型处理
    switch (my_valid_data.data[0]) {
        case 0xF1: // 收到其他设备的AT指令
            if (my_valid_data.data[5] == calcrc_data(my_valid_data.data, 5)) {
                if (valid_data_callback != NULL) {
                    valid_data_callback(&my_valid_data);
                }
            }
            break;

        case 0xF2: // 单发串码
            if (enter_config) {
                if (my_valid_data.data[24] == calcrc_data(my_valid_data.data, 24)) {
                    uint8_t last_crc[24] = {0};
                    memcpy(last_crc, &my_valid_data.data[25], 9);

                    if (last_crc[9] == calcrc_data(last_crc, 9)) {
                        app_save_config(&my_valid_data);
                    }
                }
            }
            break;

        case 0xF3: // 群发串码
            if (my_valid_data.data[24] == calcrc_data(my_valid_data.data, 24)) {
                uint8_t last_crc[24] = {0};
                memcpy(last_crc, &my_valid_data.data[25], 9);

                if (last_crc[9] == calcrc_data(last_crc, 9)) {
                    app_save_config(&my_valid_data);
                }
            }
            break;

        case 0xF8: // 设置软件回复
            if (my_valid_data.data[1] == 0x02 && my_valid_data.data[2] == 0x06) {
                app_eventbus_publish(EVENT_ENTER_CONFIG, NULL);
                enter_config = true;
            }
            break;

        case 0xF9: // 退出配置模式
            if (my_valid_data.data[1] == 0x03 && my_valid_data.data[2] == 0x04) {
                app_eventbus_publish(EVENT_EXIT_CONFIG, NULL);
                enter_config = false;
            }
            break;

        default:
            break;
    }
}

void app_save_config(valid_data_t *boj)
{
    APP_PRINTF_BUF("config_data:", boj->data, boj->length);

    uint32_t output[32] = {0};
    if (app_uint8_to_uint32(boj->data, boj->length, output, sizeof(output)) == true) {
        if (app_flash_program(CONFIG_START_ADDR, output, sizeof(output), true) == FMC_READY) {
            if (app_load_config() != true) {
                APP_PRINTF("app_load_config error\n");
            }
        }
    }
}

// 构造发送 AT 帧
void app_at_send(uint8_t *data, uint16_t len)
{
    // 转换为十六进制字符串
    char hex_buffer[125] = {0};
    char at_frame[256]   = {0};
    for (int i = 0; i < len; i++) {
        snprintf(hex_buffer + 2 * i, 3, "%02X", data[i]);
    }

    snprintf(at_frame, sizeof(at_frame), "%s,%d,\"%s\",1\r\n", AT_HEAD, len * 2, hex_buffer);
    APP_PRINTF("[send]:%s\n", at_frame);
    app_usart0_send_string(at_frame);
}
// 构造 panel 命令
void app_panel_send_cmd(uint8_t key_number, uint8_t key_status, uint8_t cmd)
{
    uint8_t frame[64] = {0};
    switch (cmd) {
        case 0xF1: { // 发送给通信帧
            frame[0] = 0XF1;

            if (key_number <= 3) { // 前4个按键
                frame[1] = my_dev_config.func[key_number];
                frame[2] = key_status;
                frame[3] = my_dev_config.group[key_number];
                frame[4] = my_dev_config.area[key_number];
                frame[6] = my_dev_config.perm[key_number];
                frame[7] = my_dev_config.scene_group[key_number];
            } else if (key_number == 4) {
                frame[1] = my_dev_config.func_5;
                frame[2] = key_status;
                frame[3] = my_dev_config.group_5;
                frame[4] = my_dev_config.area_5;
                frame[6] = my_dev_config.perm_5;
                frame[7] = my_dev_config.scene_group_5;
            } else if (key_number == 5) {
                frame[1] = my_dev_config.func_6;
                frame[2] = key_status;
                frame[3] = my_dev_config.group_6;
                frame[4] = my_dev_config.area_6;
                frame[6] = my_dev_config.perm_6;
                frame[7] = my_dev_config.scene_group_6;
            } else {
                APP_ERROR("key_number");
                return;
            }
            frame[5] = calcrc_data(frame, 6);
            app_at_send(frame, 8);
        } break;
        case 0xF8: { // 进入设置模式
            frame[0] = 0XF8;
            frame[1] = 0x01;
            frame[2] = 0x07;
            app_at_send(frame, 3);
        }
        default:
            return;
    }
}

uint8_t calcrc_data(uint8_t *rxbuf, uint8_t len)
{
    uint8_t i, sum = 0;
    for (i = 0; i < len; i++) sum = sum + rxbuf[i];
    return (0xff - sum + 1);
}

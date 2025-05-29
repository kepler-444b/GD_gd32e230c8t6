#include "gd32e23x.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "protocol.h"
#include "../uart/uart.h"
#include "../base/debug.h"
#include "../flash/flash.h"
#include "../base/base.h"
#include "../eventbus/eventbus.h"

#define CALC_CRC(rxbuf, len) ({                                 \
    uint8_t _sum = 0;                                           \
    for (uint8_t _i = 0; _i < (len); _i++) _sum += (rxbuf)[_i]; \
    (uint8_t)(0xFF - _sum + 1);                                 \
})

static valid_data_t my_valid_data = {0};
bool enter_config                 = false;

// 函数声明
void app_proto_check(uart_rx_buffer_t *my_uart_rx_buffer);
void app_save_config(valid_data_t *boj);
uint8_t calcrc_data(uint8_t *rxbuf, uint8_t len);

void app_proto_init(void)
{
    app_usart_rx_callback(app_proto_check);
}


// usart 接收到数据首先回调在这里处理
void app_proto_check(uart_rx_buffer_t *my_uart_rx_buffer)
{
    APP_PRINTF("[RECV]:%s\n", my_uart_rx_buffer->buffer);
    memset(&my_valid_data, 0, sizeof(my_valid_data));

    // 检查协议头
    if (strncmp((char *)my_uart_rx_buffer->buffer, "+RECV:", 6) != 0) {
        return;
    }

    // 查找有效数据边界
    char *start_ptr = strchr((char *)my_uart_rx_buffer->buffer, '"');
    // 检查是否找到起始引号，并且后面还有数据
    if (start_ptr == NULL || *(start_ptr + 1) == '\0') {
        return;
    }

    char *end_ptr = strchr(start_ptr + 1, '"');
    // 检查是否找到结束引号，并且位置合理
    if (end_ptr == NULL || end_ptr <= start_ptr + 1) {
        return;
    }

    // 计算数据长度并验证是否为偶数个十六进制字符
    size_t hex_len = end_ptr - start_ptr - 1;
    if (hex_len == 0 || hex_len % 2 != 0) {
        return;
    }

    // 提取并转换十六进制数据
    my_valid_data.length = hex_len / 2;
    for (uint16_t i = 0; i < my_valid_data.length; i++) {
        my_valid_data.data[i] = HEX_TO_BYTE(start_ptr + 1 + i * 2);
    }

    // 根据命令类型处理
    switch (my_valid_data.data[0]) {
        case 0xF1: // 收到其他设备的AT指令
            if (my_valid_data.data[5] == CALC_CRC(my_valid_data.data, 5)) {
                app_eventbus_publish(EVENT_RECEIVE_CMD, &my_valid_data);
            }
            break;
        case 0xF2: // 单发串码
            if (enter_config) {
                if (my_valid_data.data[24] == CALC_CRC(my_valid_data.data, 24)) {
                    uint8_t last_crc[24] = {0};
                    memcpy(last_crc, &my_valid_data.data[25], 9);
                    if (last_crc[9] == CALC_CRC(last_crc, 9)) {
                        app_save_config(&my_valid_data);
                    }
                }
            }
            break;
        case 0xF3: // 群发串码
            if (my_valid_data.data[24] == CALC_CRC(my_valid_data.data, 24)) {
                uint8_t last_crc[24] = {0};
                memcpy(last_crc, &my_valid_data.data[25], 9);

                if (last_crc[9] == CALC_CRC(last_crc, 9)) {
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
                APP_ERROR("app_load_config error");
            }
        }
    }
}

// 构造发送 AT 帧
void app_at_send(at_frame_t *my_at_frame)
{
    // 转换为十六进制字符串
    static char hex_buffer[125] = {0};
    static char at_frame[256]   = {0};
    for (int i = 0; i < my_at_frame->length; i++) {
        snprintf(hex_buffer + 2 * i, 3, "%02X", my_at_frame->data[i]);
    }

    snprintf(at_frame, sizeof(at_frame), "%s,%d,\"%s\",1\r\n", AT_HEAD, my_at_frame->length * 2, hex_buffer);
    APP_PRINTF("[send]:%s\n", at_frame);
    app_usart_tx_string(at_frame);
}
// 构造 panel 命令
void app_panel_send_cmd(uint8_t key_number, uint8_t key_status, uint8_t cmd)
{
    static at_frame_t my_at_frame = {NULL};

    switch (cmd) {
        case 0xF1: { // 发送给通信帧
            my_at_frame.data[0] = 0XF1;

            if (key_number <= 3) { // 前4个按键
                my_at_frame.data[1] = my_dev_config.func[key_number];
                my_at_frame.data[2] = key_status;
                my_at_frame.data[3] = my_dev_config.group[key_number];
                my_at_frame.data[4] = my_dev_config.area[key_number];
                my_at_frame.data[6] = my_dev_config.perm[key_number];
                my_at_frame.data[7] = my_dev_config.scene_group[key_number];
            } else if (key_number == 4) {
                my_at_frame.data[1] = my_dev_config.func_5;
                my_at_frame.data[2] = key_status;
                my_at_frame.data[3] = my_dev_config.group_5;
                my_at_frame.data[4] = my_dev_config.area_5;
                my_at_frame.data[6] = my_dev_config.perm_5;
                my_at_frame.data[7] = my_dev_config.scene_group_5;
            } else if (key_number == 5) {
                my_at_frame.data[1] = my_dev_config.func_6;
                my_at_frame.data[2] = key_status;
                my_at_frame.data[3] = my_dev_config.group_6;
                my_at_frame.data[4] = my_dev_config.area_6;
                my_at_frame.data[6] = my_dev_config.perm_6;
                my_at_frame.data[7] = my_dev_config.scene_group_6;
            } else {
                APP_ERROR("key_number");
                return;
            }
            my_at_frame.data[5] = calcrc_data(my_at_frame.data, 5);
            my_at_frame.length  = 8;
            app_at_send(&my_at_frame);
        } break;
        case 0xF8: { // 进入设置模式
            my_at_frame.data[0] = 0XF8;
            my_at_frame.data[1] = 0x01;
            my_at_frame.data[2] = 0x07;
            my_at_frame.length = 3;
            app_at_send(&my_at_frame);
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

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
#include "../config/config.h"
#include "../device/device_manager.h"

// 用于 panel 帧的校验
#define CALC_CRC(rxbuf, len) ({                                 \
    uint8_t _sum = 0;                                           \
    for (uint8_t _i = 0; _i < (len); _i++) _sum += (rxbuf)[_i]; \
    (uint8_t)(0xFF - _sum + 1);                                 \
})

static uint8_t extend_data[16]    = {0}; // 灯控面板的第二个校验值所校验的数据
static valid_data_t my_valid_data = {0};
// 此结构体用于产品配置相关
typedef struct
{
    bool apply_cmd;   // 申请配置
    bool enter_apply; // 申请回应
    bool is_ex;
} apply_t;
static apply_t my_apply;

// 函数声明
void app_proto_check(uart_rx_buffer_t *my_uart_rx_buffer);
void app_save_config(valid_data_t *boj, bool is_ex);
uint16_t calcrc_data_quick(uint8_t *rxbuf, uint8_t len);
void app_proto_process(valid_data_t *my_valid_data);

void app_proto_init(void)
{
    app_usart_rx_callback(app_proto_check);
}

// usart 接收到数据首先回调在这里处理
void app_proto_check(uart_rx_buffer_t *my_uart_rx_buffer)
{
    if (strncmp((char *)my_uart_rx_buffer->buffer, "OK", 2) == 0) {
        is_offline = true;
    }
    // 检查协议头
    if (strncmp((char *)my_uart_rx_buffer->buffer, "+RECV:", 6) == 0) {
        is_offline = false;
    } else {
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
    app_proto_process(&my_valid_data);
}

void app_proto_process(valid_data_t *my_valid_data)
{
    APP_PRINTF_BUF("[recv]", my_valid_data->data, my_valid_data->length);
    switch (my_valid_data->data[0]) {
        case PANEL_HEAD: // 收到其他设备的AT指令
            if (my_valid_data->data[5] == CALC_CRC(my_valid_data->data, 5)) {
                app_eventbus_publish(EVENT_RECEIVE_CMD, my_valid_data);
            }
            break;
        case PANEL_SINGLE: // panel 串码(单发)
            if (my_apply.enter_apply == true) {
                if (my_valid_data->data[24] == CALC_CRC(my_valid_data->data, 24)) {
                    memcpy(extend_data, &my_valid_data->data[25], 9);
                    if (extend_data[9] == CALC_CRC(extend_data, 9)) {
                        app_save_config(my_valid_data, my_apply.is_ex);
                    }
                }
            }
            break;
        case PANEL_MULTI: // panel 串码(群发)
            if (my_valid_data->data[24] == CALC_CRC(my_valid_data->data, 24)) {
                memcpy(extend_data, &my_valid_data->data[25], 9);
                if (extend_data[9] == CALC_CRC(extend_data, 9)) {
                    app_save_config(my_valid_data, false);
                }
            }
            break;
        case QUICK_SINGLE: // quick 串码(单发)
            if (my_apply.enter_apply == true) {
                if ((my_valid_data->data[1] * 11 + 5) == my_valid_data->length) { // 确定长度
                    uint16_t crc = (my_valid_data->data[my_valid_data->length - 2] << 8) | my_valid_data->data[my_valid_data->length - 1];
                    if (crc == calcrc_data_quick(my_valid_data->data, my_valid_data->length - 2)) {
                        app_save_config(my_valid_data, false);
                    }
                }
            }

            break;
        case APPLY_CONFIG: // 设置软件回复(若不是本设备发送的申请,则屏蔽软件的回复)
            if (my_valid_data->data[1] == 0x02 && my_valid_data->data[2] == 0x06 && my_apply.apply_cmd) {
                app_eventbus_publish(my_apply.is_ex ? EVENT_ENTER_CONFIG_EX : EVENT_ENTER_CONFIG, NULL);
                my_apply.enter_apply = true;
            }
            break;
        case EXIT_CONFIG: // 接收上位机发送退出命令
            if (my_valid_data->data[1] == 0x03 && my_valid_data->data[2] == 0x04 && my_apply.enter_apply) {
                app_eventbus_publish(my_apply.is_ex ? EVENT_EXIT_CONFIG_EX : EVENT_EXIT_CONFIG, NULL);
                my_apply.enter_apply = false;
                my_apply.apply_cmd   = false;
            }
            break;
        default:
            break;
    }
}

void app_save_config(valid_data_t *boj, bool is_ex)
{
    static uint32_t output[24] = {0};
    if (app_uint8_to_uint32(boj->data, boj->length, output, sizeof(output)) == true) {

        __disable_irq(); // flash 写操作,需要关闭中断
        uint32_t flash_addr = is_ex ? CONFIG_EXTEN_ADDR : CONFIG_START_ADDR;
        if (app_flash_program(flash_addr, output, sizeof(output), true) != FMC_READY) {
            APP_ERROR("app_flash_program");
        }
        __enable_irq();
    }
    app_eventbus_publish(is_ex ? EVENT_SAVE_SUCCESS_EX : EVENT_SAVE_SUCCESS, NULL); // 发布事件
}

// 构造发送 AT 帧
void app_at_send(at_frame_t *my_at_frame)
{
    if (is_offline == true) { // 如果设备离线,则把数据发送给自己
        app_eventbus_publish(EVENT_RECEIVE_CMD, my_at_frame);
    }
    APP_PRINTF_BUF("[send]", my_at_frame->data, my_at_frame->length);
    // 转换为十六进制字符串
    static char hex_buffer[125] = {0};
    static char at_frame[256]   = {0};
    for (int i = 0; i < my_at_frame->length; i++) {
        snprintf(hex_buffer + 2 * i, 3, "%02X", my_at_frame->data[i]);
    }

    snprintf(at_frame, sizeof(at_frame), "%s,%d,\"%s\",1\r\n", AT_HEAD, my_at_frame->length * 2, hex_buffer);
    app_usart_tx_string(at_frame);
}

// 构造命令
void app_send_cmd(uint8_t key_number, uint8_t key_status, uint8_t cmd, uint8_t func, bool is_ex)
{
#if defined PANEL_KEY
#ifndef PANEL_8KEY
    const panel_cfg_t *temp_cfg = app_get_panel_cfg();
#endif
#if defined PANEL_8KEY
    const panel_cfg_t *temp_cfg = is_ex ? app_get_panel_cfg_ex() : app_get_panel_cfg();
#endif
#endif
    static at_frame_t my_at_frame = {0};

    switch (cmd) {
        case PANEL_HEAD: { // 发送通信帧(panel产品用到)
#if defined PANEL_KEY
            my_at_frame.data[0] = PANEL_HEAD;

            // 验证按键编号有效性
            if (key_number > KEY_NUMBER_COUNT) {
                APP_ERROR("key_number error: %d", key_number);
                return;
            }

            if (func == CURTAIN_STOP) { // 特殊命令(窗帘开关)
                my_at_frame.data[1] = CURTAIN_STOP;
                my_at_frame.data[2] = 0x00;
            } else if (func == 0x00) { // 通用命令
                uint8_t func_value = temp_cfg[key_number].key_func;

                my_at_frame.data[1] = func_value;
                my_at_frame.data[2] = key_status;
            }

            // 设置分组、区域、权限和场景组信息
            my_at_frame.data[3] = temp_cfg[key_number].key_group;
            my_at_frame.data[4] = temp_cfg[key_number].key_area;
            my_at_frame.data[6] = temp_cfg[key_number].key_perm;
            my_at_frame.data[7] = temp_cfg[key_number].key_scene_group;

            // 计算并设置CRC校验
            my_at_frame.data[5] = CALC_CRC(my_at_frame.data, 5);
            my_at_frame.length  = 8;
            app_at_send(&my_at_frame);
#endif
        } break;

        case APPLY_CONFIG: { // 申请进入设置模式(所有产品通用)
            my_at_frame.data[0] = APPLY_CONFIG;
            my_at_frame.data[1] = 0x01;
            my_at_frame.data[2] = 0x07;
            my_at_frame.length  = 3;
            app_at_send(&my_at_frame);
            my_apply.apply_cmd = true;

            my_apply.is_ex = is_ex;
        } break;
        default:
            return;
    }
}

// 用于快装盒子的校验
uint16_t calcrc_data_quick(uint8_t *rxbuf, uint8_t len)
{
    uint16_t wcrc = 0XFFFF; // 16位crc寄存器预置
    uint8_t temp;
    uint8_t CRC_L; // 定义数组
    uint8_t CRC_H;

    uint16_t i = 0, j = 0;    // 计数
    for (i = 0; i < len; i++) // 循环计算每个数据
    {
        temp = *rxbuf & 0X00FF; // 将八位数据与crc寄存器亦或
        rxbuf++;                // 指针地址增加，指向下个数据
        wcrc ^= temp;           // 将数据存入crc寄存器
        for (j = 0; j < 8; j++) // 循环计算数据的
        {
            if (wcrc & 0X0001) // 判断右移出的是不是1，如果是1则与多项式进行异或。
            {
                wcrc >>= 1;     // 先将数据右移一位
                wcrc ^= 0XA001; // 与上面的多项式进行异或
            } else              // 如果不是1，则直接移出
            {
                wcrc >>= 1; // 直接移出
            }
        }
    }

    CRC_L = wcrc & 0xff; // crc的低八位
    CRC_H = wcrc >> 8;   // crc的高八位
    return ((CRC_L << 8) | CRC_H);
}

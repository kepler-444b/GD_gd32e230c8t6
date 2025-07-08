#include "gd32e23x.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "protocol.h"
#include "../usart/usart.h"
#include "../base/debug.h"
#include "../flash/flash.h"
#include "../base/base.h"
#include "../eventbus/eventbus.h"
#include "../config/config.h"
#include "../device/device_manager.h"

// 用于 panel 帧的校验
#define PANEL_CRC(rxbuf, len) ({                                \
    uint8_t _sum = 0;                                           \
    for (uint8_t _i = 0; _i < (len); _i++) _sum += (rxbuf)[_i]; \
    (uint8_t)(0xFF - _sum + 1);                                 \
})

#if defined PANEL_KEY
static uint8_t extend_data[16] = {0}; // 灯控面板的第二个校验值所校验的数据
#endif

#if defined QUICK_BOX
static valid_data_t temp_save = {0}; // 快装盒子存储配置信息用到的临时缓冲区
#endif

// 此结构体用于产品配置相关
typedef struct
{
    bool apply_cmd;   // 申请配置
    bool enter_apply; // 申请回应
    bool is_ex;       // 扩展配置(在8键面板中用到)
} apply_t;
static apply_t my_apply;

// 函数声明
static void app_proto_check(usart0_rx_buf_t *buf);
static void app_proto_test(usart1_rx_buf_t *buf);
static void app_save_panel_cfg(valid_data_t *boj, bool is_ex);
static void app_save_quick_cfg(valid_data_t *obj);
static void app_proto_process(valid_data_t *my_valid_data);
static void app_at_send(at_frame_t *my_at_frame);
static uint16_t quick_crc(uint8_t *data, uint8_t len);

void app_proto_init(void)
{
    app_usart0_rx_callback(app_proto_check);
    app_usart1_rx_callback(app_proto_test);
}

// usart1 接收到的数据首先回调在这里处理
static void app_proto_test(usart1_rx_buf_t *buf)
{
    APP_PRINTF("%s\n", buf->buffer);
    // app_usart_tx_string((const char *)buf->buffer, USART1);
}

// usart0 接收到数据首先回调在这里处理
static void app_proto_check(usart0_rx_buf_t *buf)
{
    // APP_PRINTF("%s\n", buf->buffer);
    if (strncmp((char *)buf->buffer, "OK", 2) == 0) {
        is_offline = true;
    }
    // 检查协议头
    if (strncmp((char *)buf->buffer, "+RECV:", 6) == 0) {
        is_offline = false;
    } else {
        return;
    }
    // 查找有效数据边界
    char *start_ptr = strchr((char *)buf->buffer, '"');
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
    static valid_data_t my_valid_data;                // 不显式初始化
    memset(&my_valid_data, 0, sizeof(my_valid_data)); // 每次调用都清零

    // 提取并转换十六进制数据
    my_valid_data.length = hex_len / 2;
    for (uint16_t i = 0; i < my_valid_data.length; i++) {
        my_valid_data.data[i] = HEX_TO_BYTE(start_ptr + 1 + i * 2);
    }
    app_proto_process(&my_valid_data);
}

static void app_proto_process(valid_data_t *my_valid_data)
{
    uint8_t cmd = my_valid_data->data[0];
    switch (cmd) {
        case PANEL_HEAD: // 收到其他设备的AT指令
            if (my_valid_data->data[5] == PANEL_CRC(my_valid_data->data, 5)) {
                app_eventbus_publish(EVENT_RECEIVE_CMD, my_valid_data);
            }
            break;
#if defined PANEL_KEY
        case PANEL_SINGLE:
        case PANEL_MULTI:
            if (cmd == PANEL_MULTI || my_apply.enter_apply) { // 群发直接通过,单发需判断是否进入了配置模式
                my_apply.enter_apply = true;                  // 这里要进入配置模式
                if (my_valid_data->data[24] != PANEL_CRC(my_valid_data->data, 24)) {
                    return;
                }
                memcpy(extend_data, &my_valid_data->data[25], 9);
                if (extend_data[9] != PANEL_CRC(extend_data, 9)) {
                    return;
                }
                app_save_panel_cfg(my_valid_data, my_apply.is_ex);
            }
            break;
#endif
#if defined QUICK_BOX
        case QUICK_SINGLE:
        case QUICK_MULTI: {
            if (cmd == QUICK_MULTI || my_apply.enter_apply) {
                my_apply.enter_apply = true;
                if (my_valid_data->data[1] != QUICK_END) {
                    uint16_t recv_crc = (my_valid_data->data[my_valid_data->length - 2] << 8) | my_valid_data->data[my_valid_data->length - 1];
                    if (recv_crc != quick_crc(my_valid_data->data, my_valid_data->length - 2)) {
                        return;
                    }
                    uint8_t addr_offset = (L_BIT(my_valid_data->data[1]) - 1) * 14; // 根据是第几路偏移内存地址
                    if (addr_offset + 14 > sizeof(temp_save.data)) {
                        return;
                    }
                    // 收到的每帧18个字节,去除前2个(帧头,地址)和后2个校验,实际需要存储的字节为14个
                    memcpy(&temp_save.data[addr_offset], &my_valid_data->data[2], 14);
                    temp_save.length = addr_offset + 14;
                } else if (my_valid_data->data[1] == QUICK_END) {
                    app_save_quick_cfg(&temp_save);
                    temp_save.length = 0;
                }
            }
        } break;
#endif
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

// 用于存放 panel 类型的串码
static void app_save_panel_cfg(valid_data_t *obj, bool is_ex)
{
#if defined PANEL_KEY
    APP_PRINTF_BUF("[SAVE]", obj->data, obj->length);
    static uint32_t output[32] = {0};
    if (app_uint8_to_uint32(obj->data, obj->length, output, sizeof(output))) {

        __disable_irq(); // flash 写操作,需要关闭中断
        uint32_t flash_addr = is_ex ? CONFIG_EXTEN_ADDR : CONFIG_START_ADDR;
        if (app_flash_program(flash_addr, output, sizeof(output), true) != FMC_READY) {
            APP_ERROR("app_flash_program");
        }
        __enable_irq();
    }
    app_eventbus_publish(is_ex ? EVENT_SAVE_SUCCESS_EX : EVENT_SAVE_SUCCESS, NULL); // 发布事件
#endif
}

// 用于存放 quick_box 类型的串码
static void app_save_quick_cfg(valid_data_t *obj)
{
#if defined QUICK_BOX
    APP_PRINTF_BUF("[SAVE]", obj->data, obj->length);
    static uint32_t output[32] = {0};
    if (app_uint8_to_uint32(obj->data, obj->length, output, sizeof(output))) {
        __disable_irq();
        uint32_t flash_addr = CONFIG_START_ADDR; // 根据 addr_offset 偏移地址存储
        if (app_flash_program(flash_addr, output, sizeof(output), true) != FMC_READY) {
            APP_ERROR("app_flash_program");
        }
        __enable_irq();
    }
    app_eventbus_publish(EVENT_SAVE_SUCCESS, NULL); // 发布事件
#endif
}
// 构造发送 AT 帧
static void app_at_send(at_frame_t *my_at_frame)
{
    APP_PRINTF_BUF("[SEND]", my_at_frame->data, my_at_frame->length);
    // 转换为十六进制字符串
    static char hex_buffer[64] = {0};
    static char at_frame[64]   = {0};
    for (int i = 0; i < my_at_frame->length; i++) {
        snprintf(hex_buffer + 2 * i, 3, "%02X", my_at_frame->data[i]);
    }

    snprintf(at_frame, sizeof(at_frame), "%s,%d,\"%s\",1\r\n", AT_HEAD, my_at_frame->length * 2, hex_buffer);
    app_usart_tx_string(at_frame, USART0); // 通过 usart0 发送到给 plc 模组

    if (is_offline) { // 如果设备离线,则把数据发送给自己
        app_eventbus_publish(EVENT_RECEIVE_CMD, my_at_frame);
    }
}

// 构造命令
void app_send_cmd(uint8_t key_number, uint8_t key_status, uint8_t frame_head, uint8_t cmd_type, bool is_ex)
{
    static at_frame_t my_at_frame;
    memset(&my_at_frame, 0, sizeof(my_at_frame));

    switch (frame_head) {
        case PANEL_HEAD: { // 发送通信帧(panel产品用到)
#if defined PANEL_KEY
            const panel_cfg_t *temp_cfg =
    #if defined PANEL_8KEY_A13
                is_ex ? app_get_panel_cfg_ex() : app_get_panel_cfg();
    #else
                app_get_panel_cfg();
    #endif
            if (key_number > KEY_NUMBER) { // 验证按键编号有效性
                APP_ERROR("key_number error: %d", key_number);
                return;
            }

            my_at_frame.data[0] = PANEL_HEAD;
            if (cmd_type == SPECIAL_CMD) {
                // 特殊命令
                if (temp_cfg[key_number].func == CURTAIN_OPEN || temp_cfg[key_number].func == CURTAIN_CLOSE) {
                    // 若是窗帘正在"开/关"过程中,则"停止"
                    my_at_frame.data[1] = CURTAIN_STOP;
                    my_at_frame.data[2] = false;
                }
            } else if (cmd_type == COMMON_CMD || (cmd_type == SPECIAL_CMD && temp_cfg[key_number].func == LATER_MODE)) {
                // 普通命令 或 特殊命令里的"请稍后"

                my_at_frame.data[1] = temp_cfg[key_number].func;
                my_at_frame.data[2] = key_status;

                if (BIT4(temp_cfg[key_number].perm) && BIT6(temp_cfg[key_number].perm)) { // "只开" + "取反"
                    my_at_frame.data[2] = false;
                } else if (BIT4(temp_cfg[key_number].perm)) { // "只开"
                    my_at_frame.data[2] = true;
                } else if (BIT6(temp_cfg[key_number].perm)) { // "取反"
                    my_at_frame.data[2] = !key_status;
                } else {
                    my_at_frame.data[2] = key_status; // 默认
                }
            }

            // 设置分组、区域、权限和场景组信息
            my_at_frame.data[3] = temp_cfg[key_number].group;
            my_at_frame.data[4] = temp_cfg[key_number].area;
            my_at_frame.data[6] = temp_cfg[key_number].perm;
            my_at_frame.data[7] = temp_cfg[key_number].scene_group;

            // 计算并设置CRC校验
            my_at_frame.data[5] = PANEL_CRC(my_at_frame.data, 5);
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
            my_apply.is_ex     = is_ex;
        } break;
        default:
            return;
    }
}

// 用于快装盒子串码的校验
static uint16_t quick_crc(uint8_t *rxbuf, uint8_t len)
{
    // CRC-16/MODBUS
    uint16_t crc = 0xFFFF;
    while (len--) {
        crc ^= *rxbuf++;
        for (uint8_t i = 0; i < 8; i++) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
        }
    }
    return (crc << 8) | (crc >> 8); // 高低字节交换
}

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

#if defined PLC_HI
    #define AT_HEAD "AT+SEND=FFFFFFFFFFFF" // AT 指令固定帧头
static bool is_offline = false;
#endif

#if defined PLC_LHW
    // #define AT_HEAD 0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x0a, 0x1b, 0x0a, 0x52, 0x02, 0x03, 0x00, 0x00, 0x33, 0xff, 0xad, 0x08, 0x40, 0x31, 0x2e
    // #define TG_MAC  0x46, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46, 0x2f, 0x31, 0x30, 0x34, 0x2f, 0x33, 0x11, 0x2a, 0xff
    #define ESCAPE_SEQ  0x1b, 0x1b, 0x1b, 0x1b, 0x1b, 0x0a, 0x1b, 0x0a
    #define MAC_ADDRESS 0x46, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46
    #define SERVICE_ID  0x31, 0x30, 0x34 // "104"

#endif

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
static void app_at_send(at_frame_t *send_frame);
static uint8_t panel_crc(uint8_t *rxbuf, uint8_t len);
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
    APP_PRINTF_BUF("", buf->buffer, buf->length);
    // app_usart_tx_string((const char *)buf->buffer, USART1);
}

// usart0 接收到数据首先回调在这里处理
static void app_proto_check(usart0_rx_buf_t *buf)
{
    // APP_PRINTF_BUF("", buf->buffer, buf->length);
    // APP_PRINTF("%s\n", buf->buffer);
    static valid_data_t my_valid_data;                // 不显式初始化
    memset(&my_valid_data, 0, sizeof(my_valid_data)); // 每次调用都清零
#if defined PLC_HI
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
    // 提取并转换十六进制数据
    my_valid_data.length = hex_len / 2;
    for (uint16_t i = 0; i < my_valid_data.length; i++) {
        my_valid_data.data[i] = HEX_TO_BYTE(start_ptr + 1 + i * 2);
    }
    app_proto_process(&my_valid_data);
#endif
#if defined PLC_LHW
    uint8_t index          = 0;
    uint8_t found_ff_count = 0;
    if (buf->buffer[0] != 0x1b && buf->buffer[7] != 0x0a) { // 验证前导码首尾
        return;
    }
    for (uint8_t i = 0; i < buf->length; i++) {
        if (buf->buffer[i] != 0xff) {
            continue;
        }
        found_ff_count++;
        if (found_ff_count == 2) { // 找到第二个 FF,后面的数据是有效数据
            index = i + 1;
            break;
        }
    }
    if (found_ff_count < 2) {
        return;
    }
    memcpy(my_valid_data.data, &buf->buffer[index], buf->length - index);
    my_valid_data.length = buf->length - index;
    app_proto_process(&my_valid_data);
#endif

#if defined PLCP_LHW
    memcpy(my_valid_data.data, buf->buffer, buf->length);
    my_valid_data.length = buf->length;
    app_eventbus_publish(EVENT_RECEIVE_CMD, &my_valid_data);
    // APP_PRINTF_BUF("rc", buf->buffer, buf->length);
#endif
}

static void app_proto_process(valid_data_t *my_valid_data)
{
    uint8_t cmd = my_valid_data->data[0];
    switch (cmd) {
        case PANEL_HEAD: // 收到其他设备的AT指令
            if (my_valid_data->data[5] == panel_crc(my_valid_data->data, 5)) {
                static at_frame_t recv_frame;
                memcpy(recv_frame.data, my_valid_data->data, my_valid_data->length);
                recv_frame.length = my_valid_data->length;
                app_eventbus_publish(EVENT_RECEIVE_CMD, &recv_frame);
            }
            break;
#if defined PANEL_KEY
        case PANEL_SINGLE:
        case PANEL_MULTI:
            if (cmd == PANEL_MULTI || my_apply.enter_apply) { // 群发直接通过,单发需判断是否进入了配置模式
                my_apply.enter_apply = true;                  // 这里要进入配置模式
                if (my_valid_data->data[24] != panel_crc(my_valid_data->data, 24)) {
                    return;
                }
                memcpy(extend_data, &my_valid_data->data[25], 9);
                if (extend_data[9] != panel_crc(extend_data, 9)) {
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
static void app_at_send(at_frame_t *send_frame)
{
    APP_PRINTF_BUF("[SEND]", send_frame->data, send_frame->length);

// 转换为十六进制字符串
#if defined PLC_HI
    static char at_frame[64]   = {0};
    static char hex_buffer[64] = {0};
    for (int i = 0; i < send_frame->length; i++) {
        snprintf(hex_buffer + 2 * i, 3, "%02X", send_frame->data[i]);
    }
    snprintf(at_frame, sizeof(at_frame), "%s,%d,\"%s\",1\r\n", AT_HEAD, send_frame->length * 2, hex_buffer);
    app_usart_tx_string(at_frame, USART0); // 通过 usart0 发送到给 plc 模组
    if (is_offline) {                      // 如果设备离线,把数据发送给自己
        app_eventbus_publish(EVENT_RECEIVE_CMD, send_frame);
        APP_ERROR("is_offline");
    }
#endif
#if defined PLC_LHW
    app_eventbus_publish(EVENT_RECEIVE_CMD, send_frame); // 先把数据发送给自己
    static uint8_t at_frame[64] = {0};

    const uint8_t escape[] = {ESCAPE_SEQ};
    memcpy(&at_frame[0], escape, sizeof(escape));
    // 报文头域 (6字节)
    at_frame[8]  = 0x52; // head
    at_frame[9]  = 0x02; // request
    at_frame[10] = 0x00; // id高字节
    at_frame[11] = 0x03; // id低字节
    at_frame[12] = 0x00; // token高字节
    at_frame[13] = 0x33; // token低字节
    // 选项域 (3字节)
    at_frame[14] = 0xff; // spe
    at_frame[15] = 0xad; // opt
    at_frame[16] = 0x08; // ext_len
    // RSL数据域 (20字节)
    at_frame[17]        = 0x40; // port_mark ("@")
    at_frame[18]        = 0x31; // port_number ("1")
    at_frame[19]        = 0x2e; // addr_spe (".")
    const uint8_t mac[] = {MAC_ADDRESS};
    memcpy(&at_frame[20], mac, sizeof(mac)); // MAC地址 (12字节)
    at_frame[32] = 0x2f;                     // path_spe ("/")

    const uint8_t service[] = {SERVICE_ID};
    memcpy(&at_frame[33], service, sizeof(service)); // 服务ID (3字节)

    at_frame[36] = 0x2f; // path_sub_spe ("/")
    at_frame[37] = 0x33; // func_sub ("3")

    // 内容选项域 (3字节)
    at_frame[38] = 0x11; // data_size
    at_frame[39] = 0x2a; // data_format
    at_frame[40] = 0xff; // end_mark
    memcpy(&at_frame[41], send_frame->data, send_frame->length);
    app_usart_tx_buf(at_frame, 41 + send_frame->length, USART0);
#endif
}

// 构造命令
void app_send_cmd(uint8_t key_number, uint8_t key_status, uint8_t frame_head, uint8_t cmd_type, bool is_ex)
{
    static at_frame_t send_frame;
    memset(&send_frame, 0, sizeof(send_frame));

    switch (frame_head) {
        case PANEL_HEAD: { // 发送通信帧(panel产品用到)
#if defined PANEL_KEY
            const panel_cfg_t *temp_cfg =
    #if defined PANEL_8KEY_A13
                is_ex ? app_get_panel_cfg_ex() : app_get_panel_cfg();
    #else
                app_get_panel_cfg();
    #endif
            send_frame.data[0] = PANEL_HEAD;
            if (cmd_type == SPECIAL_CMD) {
                // 特殊命令
                if (temp_cfg[key_number].func == CURTAIN_OPEN || temp_cfg[key_number].func == CURTAIN_CLOSE) {
                    // 若是窗帘正在"开/关"过程中,则"停止"
                    send_frame.data[1] = CURTAIN_STOP;
                    send_frame.data[2] = false;
                }
            } else if (cmd_type == COMMON_CMD || (cmd_type == SPECIAL_CMD && temp_cfg[key_number].func == LATER_MODE)) {
                // 普通命令 或 特殊命令里的"请稍后"
                send_frame.data[1] = temp_cfg[key_number].func;
                send_frame.data[2] = key_status;
                if (BIT4(temp_cfg[key_number].perm) && BIT6(temp_cfg[key_number].perm)) { // "只开" + "取反"
                    send_frame.data[2] = false;
                } else if (BIT4(temp_cfg[key_number].perm)) { // "只开"
                    send_frame.data[2] = true;
                } else if (BIT6(temp_cfg[key_number].perm)) { // "取反"
                    send_frame.data[2] = !key_status;
                } else {
                    send_frame.data[2] = key_status; // 默认
                }
            }
            // 设置分组、区域、权限和场景组信息
            send_frame.data[3] = temp_cfg[key_number].group;
            send_frame.data[4] = temp_cfg[key_number].area;
            send_frame.data[6] = temp_cfg[key_number].perm;
            send_frame.data[7] = temp_cfg[key_number].scene_group;
            // 计算并设置CRC校验
            send_frame.data[5] = panel_crc(send_frame.data, 5);
            send_frame.length  = 8;
            app_at_send(&send_frame);
#endif
        } break;
        case APPLY_CONFIG: { // 申请进入设置模式(所有产品通用)
            send_frame.data[0] = APPLY_CONFIG;
            send_frame.data[1] = 0x01;
            send_frame.data[2] = 0x07;
            send_frame.length  = 3;
            app_at_send(&send_frame);
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

// 用于 panel 帧的校验
static uint8_t panel_crc(uint8_t *rxbuf, uint8_t len)
{
    uint8_t sum = 0;
    for (uint8_t i = 0; i < len; i++) {
        sum += rxbuf[i];
    }
    return (uint8_t)(0xFF - sum + 1);
}

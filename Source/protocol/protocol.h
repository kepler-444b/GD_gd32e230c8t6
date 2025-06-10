#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#define AT_HEAD "AT+SEND=FFFFFFFFFFFF" // AT 指令固定帧头

// 用于保存接收到的有效数据
typedef struct
{
    uint8_t data[160];
    uint8_t length;

} valid_data_t;

// 用于构造发送的at帧
typedef struct
{
    uint8_t data[64];
    uint8_t length;
} at_frame_t;

typedef void (*ValidDataCallback)(valid_data_t *data);

void app_proto_init(void);
void app_at_send(at_frame_t *at_frame_t);
void app_send_cmd(uint8_t key_number, uint8_t key_status, uint8_t cmd, uint8_t func);
#endif
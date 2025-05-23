#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#define AT_HEAD "AT+SEND=FFFFFFFFFFFF" // AT 指令固定帧头

typedef struct
{
    // 用于保存接收到的有效数据
    uint8_t data[128];
    uint16_t length;

} valid_data_t;

typedef void (*ValidDataCallback)(valid_data_t *data);
void app_valid_data_callback(ValidDataCallback callback);

void app_proto_init(void);
void app_at_send(uint8_t *data, uint16_t len);
void app_panel_send_cmd(uint8_t key_number, uint8_t key_status, uint8_t cmd);
#endif
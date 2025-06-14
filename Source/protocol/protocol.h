#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include <stdbool.h>

/*
    2025.6.14 舒东升
    本模块用于设备与PLC模组通信的协议
    面板按下按键发送数据会先到CCO,然后由CCO广播给所有STA,
    面板按下按键,马上点亮自身的LED和继电器,其他STA则是收到CCO的广播后再动作,
    这样会有短暂的延时,并不能做到同步,
    故而采用这种方式:面板按下按键只是发送命令而不动作,等到CCO收到命令后广播给所有STA,
    此时片板也收到了广播命令,再执行动作,这样再感官上更加的同步,
    考虑到CCO失灵,或者自身掉线而导致收不到CCO的广播,采用了"is_offline"作为标记,
    如果按下按键发送命令,没有收到CCO的回复,则进入"无头模式",会在"构造发送 AT 帧"的时候通过
    app_eventbus_publish 把数据发送给自己,然后执行动作
*/
#define AT_HEAD "AT+SEND=FFFFFFFFFFFF" // AT 指令固定帧头

static bool is_offline = true;

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
void app_send_cmd(uint8_t key_number, uint8_t key_status, uint8_t cmd, uint8_t func, bool is_ex);
#endif
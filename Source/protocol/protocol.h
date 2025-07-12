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

// 用于保存接收到的有效数据
typedef struct
{
    uint8_t data[128];
    uint8_t length;

} valid_data_t;

// 用于构造发送的at帧
typedef struct
{
    uint8_t data[32];
    uint8_t length;
} at_frame_t;

void app_proto_init(void);
void app_send_cmd(uint8_t key_number, uint8_t key_status, uint8_t frame_head, uint8_t cmd_type, bool is_ex);

typedef enum {

    // 命令类型
    COMMON_CMD  = 0x00, // 普通命令
    SPECIAL_CMD = 0x01, // 特殊命令

    QUICK_SINGLE = 0xE2, // quick (快装盒子)单发串码
    QUICK_MULTI  = 0xE3, // quick (快装盒子)群发串码
    QUICK_END    = 0x30,
    // 面板帧头
    PANEL_HEAD   = 0xF1,
    PANEL_SINGLE = 0xF2, // panel (面板)单发串码
    PANEL_MULTI  = 0xF3, // panel (面板)群发串码
    APPLY_CONFIG = 0xF8, // 设置软件回复设置申请
    EXIT_CONFIG  = 0xF9, // 设置软件"退出"配置模式
    TEST_CMD     = 0xF2,

    // 按键功能
    ALL_CLOSE     = 0x00, // 总关
    ALL_ON_OFF    = 0x01, // 总开关
    CLEAN_ROOM    = 0x02, // 清理模式
    DND_MODE      = 0x03, // 勿扰模式
    LATER_MODE    = 0x04, // 请稍后
    CHECK_OUT     = 0x05, // 退房
    SOS_MODE      = 0x06, // 紧急呼叫
    SERVICE       = 0x07, // 请求服务
    CURTAIN_OPEN  = 0x10, // 窗帘开
    CURTAIN_CLOSE = 0x11, // 窗帘关
    SCENE_MODE    = 0x0D, // 场景模式
    LIGHT_MODE    = 0x0E, // 灯控模式
    NIGHT_LIGHT   = 0x0F, // 夜灯模式
    LIGHT_UP      = 0x18, // 调光上键
    LIGHT_DOWN    = 0x1B, // 调光下键
    DIMMING_3     = 0x1E, // 调光3
    DIMMING_4     = 0x21, // 调光4
    UNLOCKING     = 0x60, // 开锁
    BLUETOOTH     = 0x61, // 蓝牙
    CURTAIN_STOP  = 0x62, // 窗帘停
    VOLUME_ADD    = 0x63, // 音量+
    VOLUME_SUB    = 0x64, // 音量-;
    PLAY_PAUSE    = 0x65, // 播放/暂停
    NEXT_SONG     = 0x66, // 下一首
    LAST_SONG     = 0x67, // 上一首
} panel_frame_e;
#endif
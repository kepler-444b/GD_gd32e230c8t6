#ifndef _EVENTBUS_H_
#define _EVENTBUS_H_

// 事件类型定义(根据需求修改)
typedef enum {

    EVENT_ONLINE,  // 设备上线
    EVENT_OFFLINE, // 设备离线

    EVENT_ENTER_CONFIG, // 进入配置模式
    EVENT_EXIT_CONFIG,  // 退出配置模式
    EVENT_SAVE_SUCCESS, // 配置信息保存成功

    EVENT_ENTER_CONFIG_EX, // 进入配置模式
    EVENT_EXIT_CONFIG_EX,  // 退出配置模式
    EVENT_SAVE_SUCCESS_EX, // 配置信息保存成功

    EVENT_SEND_CMD,    // 面板发送命令
    EVENT_RECEIVE_CMD, // 模块接收命令

    EVENT_PLCP_NOTIFY, // PLCP 发送事件
    EVENT_PLCP_INIT_TIMER,
    EVENT_PLCP_SYCN_FLAG,

    EVENT_GET_MAC, // 获取mac地址
    EVENT_SET_DID,

    // 添加更多事件类型...
    EVENT_COUNT // 自动计算事件数量
} event_type_e;

// 事件回调函数类型(带事件类型和参数)
typedef void (*EventHandler)(event_type_e event, void *params);

// 事件结构体(包含事件类型和参数)
typedef struct
{
    event_type_e type;
    void *params;
} event_t;

// 初始化事件总线
void app_eventbus_init(void);

// 订阅所有事件(只需调用一次)
void app_eventbus_subscribe(EventHandler handler);

// 发布事件(带参数)
void app_eventbus_publish(event_type_e event, void *params);

// 处理事件(在主循环中调用)
void app_eventbus_poll(void);
#endif
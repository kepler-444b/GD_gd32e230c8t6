#ifndef _TIMER_H_
#define _TIMER_H_

#include <stdint.h>
#include <stdbool.h>

/*
    2025.5.15 舒东升
    定时器模块,最多支持10个定时器(不可以嵌套使用)
*/

typedef void (*SoftTimerCallback)(void);  // 定时器回调函数类型

typedef struct {
    bool active;                // 定时器是否激活
    bool repeat;                // 是否循环定时
    bool pending;               // 回调待处理标志
    uint32_t start_time;        // 定时器启动时的基准时间
    uint32_t interval_ms;       // 定时器间隔(ms)
    SoftTimerCallback callback; // 回调函数
    
} SoftTimer;

/// @brief 创建一个定时器
/// @param interval_ms 定时周期,单位(ms)
/// @param callback 到达定时时间触发回调函数
/// @param repeat   是否循环定时
/// @return 
int app_timer_start(uint32_t interval_ms, SoftTimerCallback callback, bool repeat);

void app_timer_init(void);              // 初始化定时器
bool app_timer_is_active(int timer_id); // 检查定时器是否激活
void app_timer_stop(int timer_id);      // 停止定时器
void app_timer_process(void);
#endif
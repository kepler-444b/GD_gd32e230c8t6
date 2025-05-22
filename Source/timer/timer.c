#include "timer.h"
#include <stdio.h>
#include "gd32e23x.h"
#include "gd32e23x_timer.h"
#include "../base/debug.h"

#define MAX_SOFT_TIMERS  10  // 最大支持的软定时器数量
#define SYSTEM_CLOCK_FREQ   72000000   // 系统时钟频率



static SoftTimer soft_timers[MAX_SOFT_TIMERS];  // 软定时器数组
static volatile uint32_t system_ticks = 0;      // 系统基准时间（毫秒）

void app_timer_init(void) 
{
    timer_parameter_struct timer_initpara;

    rcu_periph_clock_enable(RCU_TIMER14);   // 开启 TIMER14 时钟
    timer_deinit(TIMER14);                  // 复位 TIMER14 时钟

    timer_initpara.prescaler = (SYSTEM_CLOCK_FREQ / 1000000) - 1; // 分频 1MHz (每us计数一次)
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;

    timer_initpara.period            = 998;     // 1000次计数 = 1ms(每1ms溢出一次)(测试后，发现延时不准，改为998以补偿，仍有误差 一分钟会快 6 ms)
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_init(TIMER14, &timer_initpara);

    // 使能 TIMER14 中断
    timer_interrupt_flag_clear(TIMER14, TIMER_INT_FLAG_UP);
    timer_interrupt_enable(TIMER14, TIMER_INT_UP);
    timer_enable(TIMER14);

    nvic_irq_enable(TIMER14_IRQn, 2); // 配置 NVIC
}

void TIMER14_IRQHandler(void) 
{
    if (timer_interrupt_flag_get(TIMER14, TIMER_INT_FLAG_UP)) 
    {
        timer_interrupt_flag_clear(TIMER14, TIMER_INT_FLAG_UP);
        system_ticks++;

        // 遍历定时器，标记到期事件
        for (int i = 0; i < MAX_SOFT_TIMERS; i++) 
        {
            if (soft_timers[i].active && !soft_timers[i].pending) 
            {
                uint32_t elapsed = system_ticks - soft_timers[i].start_time;
                if (elapsed >= soft_timers[i].interval_ms) 
                {
                    soft_timers[i].pending = true; // 标记待处理

                    // 更新定时器状态
                    if (soft_timers[i].repeat) 
                    {
                        soft_timers[i].start_time += soft_timers[i].interval_ms;
                    } 
                    else 
                    {
                        soft_timers[i].active = false;
                    }
                }
            }
        }
    }
}

int app_timer_start(uint32_t interval_ms, SoftTimerCallback callback, bool repeat, void* arg) 
{
    if (!callback || interval_ms == 0) return -1;

    uint32_t flags = __get_PRIMASK();
    __disable_irq();

    int id = -1;
    for (int i = 0; i < MAX_SOFT_TIMERS; i++) {
        if (!soft_timers[i].active) {
            soft_timers[i] = (SoftTimer){
                .active = true,
                .repeat = repeat,
                .pending = false,
                .start_time = system_ticks,
                .interval_ms = interval_ms,
                .callback = callback,
                .user_arg = arg,
            };
            id = i;
            break;
        }
    }
    __set_PRIMASK(flags);
    return id;
}

void app_timer_stop(int timer_id) 
{
    if (timer_id >= 0 && timer_id < MAX_SOFT_TIMERS) 
    {
        uint32_t flags = __get_PRIMASK();
        __disable_irq();
        soft_timers[timer_id].active = false;
        soft_timers[timer_id].pending = false;
        __set_PRIMASK(flags);
    }
}

bool app_timer_is_active(int timer_id) 
{
    if (timer_id >= 0 && timer_id < MAX_SOFT_TIMERS) 
    {
        return soft_timers[timer_id].active;
    }
    return false;
}

void app_timer_poll(void) {
    for (int i = 0; i < MAX_SOFT_TIMERS; i++) 
    {
        if (soft_timers[i].pending) {
            // 原子操作确保pending状态正确
            uint32_t flags = __get_PRIMASK();
            __disable_irq();
            bool pending = soft_timers[i].pending;
            soft_timers[i].pending = false;
            __set_PRIMASK(flags);

            if (pending && soft_timers[i].callback) {
                soft_timers[i].callback(soft_timers[i].user_arg);
            }
        }
    }
}
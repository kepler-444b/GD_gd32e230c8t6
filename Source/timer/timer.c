#include "timer.h"
#include <stdio.h>
#include "gd32e23x.h"
#include "gd32e23x_timer.h"
#include "../base/debug.h"

#define SYSTEM_CLOCK_FREQ 72000000 ///< 系统时钟频率
#define TIMER_PERIOD      998      ///< 定时器周期(1ms中断)

static soft_timer_t my_soft_timer[MAX_SOFT_TIMERS]; ///< 软定时器数组
static volatile uint32_t system_ticks = 0;          ///< 系统基准时间(毫秒),定时时间不得超过49.7天

/**
 * @brief 查找空闲定时器
 * @return 空闲定时器索引, -1表示无可用定时器
 */
static int find_free_timer(void)
{
    for (int i = 0; i < MAX_SOFT_TIMERS; i++) {
        if (my_soft_timer[i].state == TIMER_STATE_INACTIVE) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 通过名称查找定时器
 * @param name 定时器名称
 * @return 定时器索引, -1表示未找到
 */
static int find_timer_by_name(const char *name)
{
    if (name == NULL || strlen(name) == 0) {
        return -1;
    }

    for (int i = 0; i < MAX_SOFT_TIMERS; i++) {
        if (my_soft_timer[i].state != TIMER_STATE_INACTIVE &&
            strcmp(my_soft_timer[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

void app_timer_init(void)
{
    timer_parameter_struct timer_initpara;

    // 初始化所有定时器状态
    for (int i = 0; i < MAX_SOFT_TIMERS; i++) {
        my_soft_timer[i].state   = TIMER_STATE_INACTIVE;
        my_soft_timer[i].name[0] = '\0';
    }

    rcu_periph_clock_enable(RCU_TIMER14); // 开启 TIMER14 时钟
    timer_deinit(TIMER14);                // 复位 TIMER14 时钟

    // 配置定时器为1ms中断
    timer_initpara.prescaler        = (SYSTEM_CLOCK_FREQ / 1000000) - 1; // 分频到1MHz
    timer_initpara.alignedmode      = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection = TIMER_COUNTER_UP;
    timer_initpara.period           = TIMER_PERIOD; // 1000次计数 = 1ms
    timer_initpara.clockdivision    = TIMER_CKDIV_DIV1;
    timer_init(TIMER14, &timer_initpara);

    // 使能中断
    timer_interrupt_flag_clear(TIMER14, TIMER_INT_FLAG_UP);
    timer_interrupt_enable(TIMER14, TIMER_INT_UP);
    timer_enable(TIMER14);

    nvic_irq_enable(TIMER14_IRQn, 1); // 配置NVIC,优先级1
}

void TIMER14_IRQHandler(void)
{
    if (timer_interrupt_flag_get(TIMER14, TIMER_INT_FLAG_UP)) {
        timer_interrupt_flag_clear(TIMER14, TIMER_INT_FLAG_UP);
        system_ticks++;

        // 遍历定时器，标记到期事件
        for (int i = 0; i < MAX_SOFT_TIMERS; i++) {
            if (my_soft_timer[i].state == TIMER_STATE_ACTIVE) {
                uint32_t elapsed = system_ticks - my_soft_timer[i].start_time;
                if (elapsed >= my_soft_timer[i].interval_ms) {
                    my_soft_timer[i].state = TIMER_STATE_PENDING; // 标记待处理

                    // 更新定时器状态
                    if (my_soft_timer[i].repeat) {
                        my_soft_timer[i].start_time += my_soft_timer[i].interval_ms;
                    } else {
                        my_soft_timer[i].state = TIMER_STATE_INACTIVE;
                    }
                }
            }
        }
    }
}

timer_error_e app_timer_start(uint32_t interval_ms, SoftTimerCallback callback, bool repeat, void *arg, const char *name)
{
    // 参数检查
    if (!callback || interval_ms == 0) {
        return TIMER_ERR_INVALID_PARAM;
    }

    // 检查名称长度
    if (name && strlen(name) >= MAX_TIMER_NAME_LEN) {
        return TIMER_ERR_NAME_TOO_LONG;
    }

    uint32_t flags = __get_PRIMASK();
    __disable_irq();

    int id = find_free_timer();
    if (id >= 0) {
        my_soft_timer[id] = (soft_timer_t){
            .state       = TIMER_STATE_ACTIVE,
            .repeat      = repeat,
            .start_time  = system_ticks,
            .interval_ms = interval_ms,
            .callback    = callback,
            .user_arg    = arg};

        // 设置定时器名称
        if (name && name[0] != '\0') {
            strncpy(my_soft_timer[id].name, name, MAX_TIMER_NAME_LEN - 1);
            my_soft_timer[id].name[MAX_TIMER_NAME_LEN - 1] = '\0';
        } else {
            my_soft_timer[id].name[0] = '\0';
        }
    }

    __set_PRIMASK(flags);
    return (id >= 0) ? TIMER_ERR_SUCCESS : TIMER_ERR_NO_RESOURCE;
}

timer_error_e app_timer_stop(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return TIMER_ERR_INVALID_PARAM;
    }

    uint32_t flags = __get_PRIMASK();
    __disable_irq();

    int id = find_timer_by_name(name);
    if (id >= 0) {
        my_soft_timer[id].state = TIMER_STATE_INACTIVE;
        __set_PRIMASK(flags);
        return TIMER_ERR_SUCCESS;
    }

    __set_PRIMASK(flags);
    return TIMER_ERR_NOT_FOUND;
}

bool app_timer_is_active(const char *name)
{
    int id = find_timer_by_name(name);
    return (id >= 0) ? (my_soft_timer[id].state == TIMER_STATE_ACTIVE) : false;
}

void app_timer_poll(void)
{
    for (int i = 0; i < MAX_SOFT_TIMERS; i++) {
        if (my_soft_timer[i].state == TIMER_STATE_PENDING) {
            // 原子操作确保状态正确
            uint32_t flags = __get_PRIMASK();
            __disable_irq();
            bool pending = (my_soft_timer[i].state == TIMER_STATE_PENDING);
            if (pending) {
                my_soft_timer[i].state = TIMER_STATE_ACTIVE;
            }
            __set_PRIMASK(flags);

            if (pending && my_soft_timer[i].callback) {
                my_soft_timer[i].callback(my_soft_timer[i].user_arg);
            }
        }
    }
}

uint32_t app_timer_get_ticks(void)
{
    return system_ticks;
}

int8_t app_timer_get_id_by_name(const char *name)
{
    return (int8_t)find_timer_by_name(name);
}
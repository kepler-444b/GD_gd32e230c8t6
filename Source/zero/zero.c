#include <stdio.h>
#include "zero.h"
#include "gd32e23x.h"
#include "gd32e23x_timer.h"
#include "../base/debug.h"
#include "../gpio/gpio.h"
#include "../eventbus/eventbus.h"

#if 0
#define SYSTEM_CLOCK_FREQ 72000000 ///< 系统时钟频率
#define TIMER_PERIOD      999      ///< 定时器周期(1ms中断)

typedef struct
{
    bool begin_zero;
    bool first_zero;
    uint64_t first_zero_count;
} zero_t;
static zero_t my_zero = {0};

void zero_event_handler(event_type_e event, void *params);
void app_zero_init(void)
{
    app_eventbus_subscribe(zero_event_handler);
    rcu_periph_clock_enable(RCU_TIMER2); // 开启 TIMER14 时钟
    timer_deinit(TIMER2);                // 复位 TIMER14 时钟

    timer_parameter_struct timer_initpara;
    // 配置定时器为1ms中断
    timer_initpara.prescaler        = (SYSTEM_CLOCK_FREQ / 1000000) - 1; // 分频到1MHz
    timer_initpara.alignedmode      = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection = TIMER_COUNTER_UP;
    timer_initpara.period           = TIMER_PERIOD; // 1000次计数 = 1ms
    timer_initpara.clockdivision    = TIMER_CKDIV_DIV2;
    timer_init(TIMER2, &timer_initpara);
    // 使能中断
    timer_interrupt_flag_clear(TIMER2, TIMER_INT_FLAG_UP);
    timer_interrupt_enable(TIMER2, TIMER_INT_UP);
    timer_enable(TIMER2);
    nvic_irq_enable(TIMER2_IRQn, 3); // 配置NVIC,优先级3
}

void TIMER2_IRQHandler(void)
{
    if (timer_interrupt_flag_get(TIMER2, TIMER_INT_FLAG_UP)) {
        timer_interrupt_flag_clear(TIMER2, TIMER_INT_FLAG_UP);

        my_zero.first_zero_count++;
        if(my_zero.first_zero_count == 1000)
        {
            APP_PRINTF("1\n");
            my_zero.first_zero_count = 0;
        }
        // if (my_zero.begin_zero == true) { // 开始采集零点信号
        //     if (APP_GET_GPIO(PB11) == true) {
        //         my_zero.first_zero = true; // 采集到第一个零点信号
        //     }
        // }

        // if (my_zero.first_zero == true) {
        //     my_zero.first_zero_count++;
        //     if (my_zero.first_zero_count == 9) {

        //         app_eventbus_publish(EVEBT_ZERO_TIME, NULL);
        //         my_zero.first_zero_count = 0;
        //         my_zero.first_zero       = false;
        //     }
        // }
    }
}

void zero_event_handler(event_type_e event, void *params)
{
    if (event == EVENT_ZERO_BEGIN) {
        my_zero.begin_zero = true;
    }
}
#endif
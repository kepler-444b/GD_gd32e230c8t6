#include "gd32e23x.h"
#include <stdio.h>
#include "watchdog.h"

// #define SYSTEM_CLOCK_FREQ 72000000 // 系统时钟频率(72MHz)
// #define TIMER_PERIOD      1999     // 2kHz下2000ms=1s中断

void app_watchdog_init(void)
{
    // TIMER_5 定时器配置
    // rcu_periph_clock_enable(RCU_TIMER5); // 开启TIMER5时钟
    // timer_deinit(TIMER5);                // 复位TIMER5

    // timer_parameter_struct timer_cfg;
    // timer_cfg.prescaler        = (SYSTEM_CLOCK_FREQ / 2000) - 1; // 分频到2kHz
    // timer_cfg.alignedmode      = TIMER_COUNTER_EDGE;
    // timer_cfg.counterdirection = TIMER_COUNTER_UP;
    // timer_cfg.period           = TIMER_PERIOD;
    // timer_cfg.clockdivision    = TIMER_CKDIV_DIV1;
    // timer_init(TIMER5, &timer_cfg);

    // timer_interrupt_flag_clear(TIMER5, TIMER_INT_FLAG_UP);
    // timer_interrupt_enable(TIMER5, TIMER_INT_UP);
    // timer_enable(TIMER5);
    nvic_irq_enable(TIMER5_IRQn, 3); // 配置NVIC,优先级3

    // 看门狗配置
    rcu_osci_on(RCU_IRC40K);             // 使能看门狗独立时钟源
    rcu_osci_stab_wait(RCU_IRC40K);      // 等待时钟稳定
    fwdgt_config(1875, FWDGT_PSC_DIV64); // 3s 溢出一次(触发中断服务函数)
    fwdgt_enable();
}

// void TIMER5_IRQHandler(void)
// {
//     if (!timer_interrupt_flag_get(TIMER5, TIMER_INT_FLAG_UP)) {
//         return;
//     }
//     timer_interrupt_flag_clear(TIMER5, TIMER_INT_FLAG_UP);
//     fwdgt_counter_reload(); // 1s 喂狗一次
// }
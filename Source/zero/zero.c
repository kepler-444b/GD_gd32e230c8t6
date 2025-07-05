#include <stdio.h>
#include "zero.h"
#include "gd32e23x.h"
#include "gd32e23x_timer.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "../base/debug.h"
#include "../eventbus/eventbus.h"

#define SYSTEM_CLOCK_FREQ 72000000 // 系统时钟频率
#define TIMER_PERIOD      99       // 定时器周期(100us触发中断)
#define DELAY_TIME        50       // 延时触发继电器 5ms(50 * 100us)
#define QUEUE_SIZE        6        // 队列容量

#define ZERO_TIMEOUT      400 // 过零超时 40ms (400 × 100us)

typedef struct {
    gpio_pin_t pin;
    bool status;
} gpio_ctrl_t;

// 队列相关
static StaticQueue_t ZeroStaticBuffer;
static QueueHandle_t ZeroQueueHandle;
static uint8_t ZeroQueueStorage[QUEUE_SIZE * sizeof(gpio_ctrl_t)];

static gpio_ctrl_t pending_cmd = {0}; // 待执行的GPIO命令

static volatile bool zero_failed            = false; // 过零检测是否失效
static volatile uint32_t zero_timeout_count = 0;     // 过零信号超时计数器

static volatile bool is_delaying     = false; // 是否正在延时
static volatile uint16_t delay_count = 0;     // 延时计数器

void app_zero_init(exti_line_enum exti_pin)
{
    // 过零检测外部中断配置(PB11)
    rcu_periph_clock_enable(RCU_CFGCMP);                           // EXIT 线路与 GPIO 相连接
    syscfg_exti_line_config(EXTI_SOURCE_GPIOB, EXTI_SOURCE_PIN11); // GPIO 配置为外部中断
    exti_init(exti_pin, EXTI_INTERRUPT, EXTI_TRIG_RISING);         // 上升沿触发
    nvic_irq_enable(EXTI4_15_IRQn, 3);

    // 定时器配置,用于精准延时(TIMER5)
    rcu_periph_clock_enable(RCU_TIMER15);
    timer_deinit(TIMER15);
    timer_parameter_struct timer_cfg;
    timer_cfg.prescaler        = (SYSTEM_CLOCK_FREQ / 1000000) - 1;
    timer_cfg.alignedmode      = TIMER_COUNTER_EDGE;
    timer_cfg.counterdirection = TIMER_COUNTER_UP;
    timer_cfg.period           = TIMER_PERIOD;
    timer_cfg.clockdivision    = TIMER_CKDIV_DIV1;
    timer_init(TIMER15, &timer_cfg);
    timer_interrupt_flag_clear(TIMER15, TIMER_INT_FLAG_UP);
    timer_interrupt_enable(TIMER15, TIMER_INT_UP);
    timer_enable(TIMER15);
    nvic_irq_enable(TIMER15_IRQn, 3);

    // 创建一个队列
    ZeroQueueHandle = xQueueCreateStatic(QUEUE_SIZE, sizeof(gpio_ctrl_t), ZeroQueueStorage, &ZeroStaticBuffer);
}

void EXTI4_15_IRQHandler(void) // 过零点触发中断服务函数
{
    if (!exti_interrupt_flag_get(EXTI_11)) {
        return; // 未捕捉到上升沿直接返回
    }
    exti_interrupt_flag_clear(EXTI_11);
    zero_timeout_count = 0; // 重置过零超时计数器

    if (is_delaying) {
        return; // 正在延时时直接返回
    }
    if (uxQueueMessagesWaiting(ZeroQueueHandle) == 0) {
        return; // 队列为空时直接返回
    }
    if (xQueueReceive(ZeroQueueHandle, &pending_cmd, 0)) { // 从队列中取命令(非阻塞)
        is_delaying = true;
        delay_count = 0;
    }
}

void TIMER15_IRQHandler(void)
{
    if (!timer_interrupt_flag_get(TIMER15, TIMER_INT_FLAG_UP)) {
        return;
    }
    timer_interrupt_flag_clear(TIMER15, TIMER_INT_FLAG_UP);

    // 检测零点电路是否可用
    if (!zero_failed) {
        if (zero_timeout_count < ZERO_TIMEOUT) {
            zero_timeout_count++;
        } else {
            zero_failed = true; // 零点检测电路标记为失效
        }
    }
    // 处理延时逻辑
    if (is_delaying) {
        delay_count++;
        if (delay_count >= DELAY_TIME) { // 达到延时时间后执行GPIO操作
            APP_SET_GPIO(pending_cmd.pin, pending_cmd.status);
            is_delaying = false;
        }
    }
}

void zero_set_gpio(const gpio_pin_t pin, bool status)
{
    if (zero_failed) { // 零点检测电路失效,直接操作继电器
        APP_SET_GPIO(pin, status);
        APP_ERROR("zero_failed");
    } else { // 正常过零延时
        gpio_ctrl_t gpio_cmd = {.pin = pin, .status = status};
        xQueueSend(ZeroQueueHandle, &gpio_cmd, 0); // 将命令发送到队列(非阻塞)
    }
}

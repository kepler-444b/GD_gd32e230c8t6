#include "gd32e23x.h"
#include <stdio.h>
#include <string.h>
#include "gd32e230c_eval.h"
#include "gd32e23x_usart.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "../base/debug.h"
#include "../base/base.h"

#if defined USE_RTT
    #include "../rtt/SEGGER_RTT.h"
#endif
// 函数声明
static void usart_common_init(uint32_t usart_periph, uint32_t baudrate);

static usart0_rx_buf_t rx0_buf = {0};
static usart1_rx_buf_t rx1_buf = {0};

static usart_rx0_callback_t rx0_callback = NULL;
static usart_rx1_callback_t rx1_callback = NULL;

void app_usart0_rx_callback(usart_rx0_callback_t callback)
{
    rx0_callback = callback;
}
void app_usart1_rx_callback(usart_rx1_callback_t callback)
{
    rx1_callback = callback;
}

void app_usart_init(uint32_t usart_periph, uint32_t baudrate)
{
    rcu_periph_clock_enable(RCU_GPIOA);

    if (usart_periph == USART0) {
        rcu_periph_clock_enable(RCU_USART0);

        gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_9 | GPIO_PIN_10);
        gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9 | GPIO_PIN_10);
        gpio_af_set(GPIOA, GPIO_AF_1, GPIO_PIN_9 | GPIO_PIN_10);
        nvic_irq_enable(USART0_IRQn, 3);

    } else if (usart_periph == USART1) {
        rcu_periph_clock_enable(RCU_USART1);

        gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_2 | GPIO_PIN_3);
        gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_2 | GPIO_PIN_3);
        gpio_af_set(GPIOA, GPIO_AF_1, GPIO_PIN_2 | GPIO_PIN_3);
        nvic_irq_enable(USART1_IRQn, 3);
    }
    usart_common_init(usart_periph, baudrate);
}

static void usart_common_init(uint32_t usart_periph, uint32_t baudrate)
{
    usart_deinit(usart_periph);
    usart_baudrate_set(usart_periph, baudrate);
    usart_word_length_set(usart_periph, USART_WL_8BIT);
    usart_stop_bit_set(usart_periph, USART_STB_1BIT);
    usart_parity_config(usart_periph, USART_PM_NONE);
    usart_hardware_flow_rts_config(usart_periph, USART_RTS_DISABLE);
    usart_hardware_flow_cts_config(usart_periph, USART_CTS_DISABLE);
    usart_transmit_config(usart_periph, USART_TRANSMIT_ENABLE);
    usart_receive_config(usart_periph, USART_RECEIVE_ENABLE);
    usart_enable(usart_periph);
    usart_interrupt_enable(usart_periph, USART_INT_RBNE);
    usart_interrupt_enable(usart_periph, USART_INT_IDLE);
}

// USART0中断服务函数
void USART0_IRQHandler(void)
{
    // 侦听非空中断
    if (usart_interrupt_flag_get(USART0, USART_INT_FLAG_RBNE) != RESET) {
        usart_interrupt_flag_clear(USART0, USART_INT_FLAG_RBNE);
        uint8_t data = (uint8_t)usart_data_receive(USART0);
        if (rx0_buf.length < UART0_RECV_SIZE) {
            rx0_buf.buffer[rx0_buf.length++] = data;
        }
    }
    // 侦听空闲中断
    if (usart_interrupt_flag_get(USART0, USART_INT_FLAG_IDLE) != RESET) {
        usart_interrupt_flag_clear(USART0, USART_INT_FLAG_IDLE);
        if (rx0_buf.length > 0 && rx0_callback != NULL) {
            rx0_callback(&rx0_buf);
            rx0_buf.length = 0;
        }
    }
}

void USART1_IRQHandler(void)
{
    // 接收缓冲区非空中断
    if (usart_interrupt_flag_get(USART1, USART_INT_FLAG_RBNE) != RESET) {
        usart_interrupt_flag_clear(USART1, USART_INT_FLAG_RBNE);
        uint8_t data = (uint8_t)usart_data_receive(USART1);
        if (rx1_buf.length < UART1_RECV_SIZE) {
            rx1_buf.buffer[rx1_buf.length++] = data;
        }
    }
    if (usart_interrupt_flag_get(USART1, USART_INT_FLAG_IDLE) != RESET) { // 侦听空闲中断
        usart_interrupt_flag_clear(USART1, USART_INT_FLAG_IDLE);
        if (rx1_buf.length > 0 && rx1_callback != NULL) {
            rx1_callback(&rx1_buf);
            rx1_buf.length = 0;
        }
    }
}

// 发送一个字节
static void usart_tx_byte(uint8_t data, uint32_t usart_periph)
{
    usart_data_transmit(usart_periph, data);
    while (RESET == usart_flag_get(usart_periph, USART_FLAG_TBE));
    if (usart_periph == USART0) {
        vTaskDelay(1);
    }
}

// 发送字符串
void app_usart_tx_string(const char *str, uint32_t usart_periph)
{
    while (*str) {
        usart_tx_byte(*str++, usart_periph);
    }
}

inline int fputc(int ch, FILE *f)
{
#ifndef USE_RTT
    usart_data_transmit(USART1, (uint8_t)ch);
    while (RESET == usart_flag_get(USART1, USART_FLAG_TBE));
#else
    SEGGER_RTT_PutChar(0, ch);
#endif
    return ch;
}

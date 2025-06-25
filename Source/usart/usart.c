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

static usart_rx_callback_t rx_callback = NULL;
void app_usart_rx_callback(usart_rx_callback_t callback)
{
    rx_callback = callback;
}

void app_usart_init(uint8_t usart_num, uint32_t baudrate)
{
    rcu_periph_clock_enable(RCU_GPIOA);

    if (usart_num == 0) {
        rcu_periph_clock_enable(RCU_USART0);

        // 配置 USART0 引脚
        gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_9 | GPIO_PIN_10);
        gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9 | GPIO_PIN_10);
        gpio_af_set(GPIOA, GPIO_AF_1, GPIO_PIN_9 | GPIO_PIN_10);

        // 配置 USART0
        usart_deinit(USART0);
        usart_baudrate_set(USART0, baudrate);
        usart_word_length_set(USART0, USART_WL_8BIT);              // 8位数据
        usart_stop_bit_set(USART0, USART_STB_1BIT);                // 1位停止位
        usart_parity_config(USART0, USART_PM_NONE);                // 无校验
        usart_hardware_flow_rts_config(USART0, USART_RTS_DISABLE); // 无硬件流控
        usart_hardware_flow_cts_config(USART0, USART_CTS_DISABLE);

        // 使能接收和发送
        usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
        usart_receive_config(USART0, USART_RECEIVE_ENABLE);
        usart_enable(USART0); // 使能 USART0

        // USART0 作为业务口，需要配置中断
        usart_interrupt_enable(USART0, USART_INT_RBNE); // 接收缓冲区非空中断使能
        usart_interrupt_enable(USART0, USART_INT_IDLE); // 空闲中断使能
        nvic_irq_enable(USART0_IRQn, 3);                // 配置 NVIC
    } else if (usart_num == 1) {
        rcu_periph_clock_enable(RCU_USART1);

        // 配置 USART1 引脚
        gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_1 | GPIO_PIN_2);
        gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_1 | GPIO_PIN_2);
        gpio_af_set(GPIOA, GPIO_AF_1, GPIO_PIN_1 | GPIO_PIN_2);

        // 配置 USART1
        usart_deinit(USART1);
        usart_baudrate_set(USART1, baudrate);
        usart_word_length_set(USART1, USART_WL_8BIT);              // 8位数据
        usart_stop_bit_set(USART1, USART_STB_1BIT);                // 1位停止位
        usart_parity_config(USART1, USART_PM_NONE);                // 无校验
        usart_hardware_flow_rts_config(USART1, USART_RTS_DISABLE); // 无硬件流控
        usart_hardware_flow_cts_config(USART1, USART_CTS_DISABLE);

        // 使能接收和发送
        usart_transmit_config(USART1, USART_TRANSMIT_ENABLE);
        usart_receive_config(USART1, USART_RECEIVE_ENABLE);
        usart_enable(USART1); // 使能USART 1
    }
}

// USART0中断服务函数
void USART0_IRQHandler(void)
{
    // 侦听非空中断
    if (usart_interrupt_flag_get(USART0, USART_INT_FLAG_RBNE) != RESET) {
        usart_interrupt_flag_clear(USART0, USART_INT_FLAG_RBNE);
        uint8_t data = (uint8_t)usart_data_receive(USART0);
        if (rx_buffer.length < UART_RECV_SIZE) {
            rx_buffer.buffer[rx_buffer.length++] = data;
        }
    }

    // 侦听空闲中断
    if (usart_interrupt_flag_get(USART0, USART_INT_FLAG_IDLE) != RESET) {
        usart_interrupt_flag_clear(USART0, USART_INT_FLAG_IDLE);
        if (rx_buffer.length > 0 && rx_callback != NULL) {
            rx_callback(&rx_buffer);
            memset(&rx_buffer, 0, sizeof(rx_buffer));
        }
    }
}

// 发送一个字节
void app_usart_tx_byte(uint8_t data)
{
    usart_data_transmit(USART0, data);
    while (RESET == usart_flag_get(USART0, USART_FLAG_TBE));
    vTaskDelay(1); // 这里必须加一个非阻塞的延时函数(原因未知,可能是plc模组的问题)
}

// 发送字符串
void app_usart_tx_string(const char *str)
{
    while (*str) {
        app_usart_tx_byte(*str++);
    }
}

// 发送指定长度的数据
void app_usart_tx_data(const uint8_t *data, uint16_t length)
{
    for (uint16_t i = 0; i < length; i++) {
        app_usart_tx_byte(data[i]); // 复用单字节发送函数
    }
}

#if 0
void app_usart_poll(void)
{
    // 检测空闲状态标志
    if (usart_flag_get(USART0, USART_FLAG_IDLE) != RESET) {
        // 清除空闲标志
        usart_data_receive(USART0);
        usart_flag_clear(USART0, USART_FLAG_IDLE);

        // 处理接收到的数据
        if (rx_buffer.length > 0 && rx_callback != NULL) {
            rx_callback(rx_buffer.buffer, rx_buffer.length);
        }
        rx_buffer.length = 0; // 重置缓冲区
    }
}
#endif

// printf 重定向到 USART1
int fputc(int ch, FILE *f)
{
    usart_data_transmit(USART1, (uint8_t)ch);
    while (RESET == usart_flag_get(USART1, USART_FLAG_TBE));

    return ch;
}

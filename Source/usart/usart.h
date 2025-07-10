#ifndef _UART_H_
#define _UART_H_

/*
    2025.5.8 舒东升
    两路 usart 的初始化,完成收发功能,
    暂使用一个固定的缓冲区 uart0_rx_buf_t,
    usart0 为业务串口,usart1 为调试串口,

    2025.5.8 舒东升
    usart1 也改为业务串口,新增了 uart1_rx_buf_t,
    增加宏定义 RTT_ENABLE,开启后,debug 会通过RTT输出,
*/

#define RTT_ENABLE      // 启用 RTT 调试

#define UART0_RECV_SIZE 256
#define UART1_RECV_SIZE 128

typedef struct {
    uint8_t buffer[UART0_RECV_SIZE];
    uint16_t length;
} usart0_rx_buf_t;

typedef struct {
    uint8_t buffer[UART1_RECV_SIZE];
    uint16_t length;
} usart1_rx_buf_t;

typedef void (*usart_rx0_callback_t)(usart0_rx_buf_t *buf);
typedef void (*usart_rx1_callback_t)(usart1_rx_buf_t *buf);
// 初始化串口
void app_usart_init(uint32_t usart_periph, uint32_t baudrate);
void app_usart_tx_string(const char *str, uint32_t usart_periph);

void app_usart0_rx_callback(usart_rx0_callback_t callback);
void app_usart1_rx_callback(usart_rx1_callback_t callback);

#endif
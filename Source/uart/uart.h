#ifndef _UART_H_
#define _UART_H_

/*
    2025.5.8 舒东升
    两路 usart 的初始化,完成收发功能,
    暂使用一个固定的缓冲区 uart_rx_buffer_t,
    usart0 为业务串口,usart1 为调试串口,
*/

#define UART_RECV_SIZE 256
typedef struct {
    uint8_t buffer[UART_RECV_SIZE];
    uint16_t length;
} uart_rx_buffer_t;

static uart_rx_buffer_t rx_buffer = {0};

typedef void (*usart_rx_callback_t)(uart_rx_buffer_t *my_uart_rx_buffer);

// 初始化串口
void app_usart_init(uint8_t usart_num, uint32_t baudrate);

// 发送单个字节
void app_usart_tx_byte(uint8_t data);

// 发送字符串
void app_usart_tx_string(const char *str);

// 发送指定长度的数据
void app_usart_tx_data(const uint8_t *data, uint16_t length);

// 业务串口 USART0 回调函数
void app_usart_rx_callback(usart_rx_callback_t callback);

void app_usart_poll(void);

// printf 重定向到 物理串口
int fputc(int ch, FILE *f);
#endif
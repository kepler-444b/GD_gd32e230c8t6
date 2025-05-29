#ifndef _UART_H_
#define _UART_H_

#define UART_RECV_SIZE 512
typedef struct {
    uint8_t buffer[UART_RECV_SIZE];
    uint16_t length;
} uart_rx_buffer_t;

static uart_rx_buffer_t rx_buffer = {0};

typedef void (*usart_rx_callback_t)(uart_rx_buffer_t *my_uart_rx_buffer);

// 初始化串口
void app_usart_init(uint8_t usart_num, uint32_t baudrate);

void app_usart_tx_byte(uint8_t data);
void app_usart_tx_string(const char *str);
void app_usart_tx_data(const uint8_t *data, uint16_t length);

// 业务串口 USART0 回调函数
void app_usart_rx_callback(usart_rx_callback_t callback);

void app_usart_poll(void);

// printf 重定向到 物理串口
int fputc(int ch, FILE *f);
#endif
#ifndef _UART_H_
#define _UART_H_ 

typedef void (*usart_rx_callback_t)(uint8_t *data, uint16_t length);

// 初始化串口
void app_usart_init(uint8_t usart_num, uint32_t baudrate);

void app_usart0_send_byte(uint8_t data);
void app_usart0_send_string(const char *str);

// 业务串口 USART0 回调函数
void app_usart0_rx_callback(usart_rx_callback_t callback);


// printf 重定向到 物理串口
int fputc(int ch, FILE *f);
#endif
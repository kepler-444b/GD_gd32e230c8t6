#include "gd32e23x.h"
#include <stdio.h>
#include "protocol.h"
#include "../uart/uart.h"
#include "../base/debug.h"
#include <string.h>

void app_proto_check(uint8_t *data, uint16_t length);

void app_proto_init(void)
{
    app_usart0_rx_callback(app_proto_check);
}

void app_proto_check(uint8_t *data, uint16_t length)
{
    APP_PRINTF_BUF("data", data, length);

    if (strncmp((char *)data, "+RECV:", 6) == 0)
    {
        uint8_t *start_ptr = (uint8_t *)strchr((char *)data, '"');
        if (start_ptr != NULL)
        {
            uint8_t *end_ptr = (uint8_t *)strchr((char *)(start_ptr + 1), '"');
            if (end_ptr != NULL)
            {
                uint16_t valid_length = end_ptr - start_ptr - 1;
                
                uint8_t valid_data[32] = {0}; // 假设最大32字节
                memcpy(valid_data, start_ptr + 1, valid_length);
                
                APP_PRINTF("Valid Data: %s\n", valid_data);
                APP_PRINTF_BUF("Valid Buffer", valid_data, valid_length);
            }
        }
    }
    else
    {
        APP_PRINTF("Invalid format, expected +RECV:\n");
    }
}
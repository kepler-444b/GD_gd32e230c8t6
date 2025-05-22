#include "gd32e23x.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "protocol.h"
#include "../uart/uart.h"
#include "../base/debug.h"
#include "../flash/flash.h"
#include "../timer/timer.h"
#include "../base/base.h"



typedef struct
{
    // 用于保存接收到的有效数据
    uint8_t data[128];
    uint16_t length;

}valid_data;
static valid_data my_valid_data = {0};

void app_proto_check(uint8_t *data, uint16_t length);
void app_save_config(valid_data* boj);
void app_proto_init(void)
{
    app_usart0_rx_callback(app_proto_check);
    
}

void app_proto_check(uint8_t *data, uint16_t length)
{
    APP_PRINTF("frame:%s", data);
    if (strncmp((char *)data, "+RECV:", 6) == 0)
    {
        uint8_t *start_ptr = (uint8_t *)strchr((char *)data, '"');
        if (start_ptr != NULL)
        {
            uint8_t *end_ptr = (uint8_t *)strchr((char *)(start_ptr + 1), '"');
            if (end_ptr != NULL)
            {
                // 这里是有效数据 "" 里的内容
                my_valid_data.length = (end_ptr - start_ptr - 1) / 2;
                for(uint16_t i = 0; i < my_valid_data.length; i++)
                {
                    my_valid_data.data[i] = HEX_TO_BYTE(start_ptr + 1 + i * 2);
                }
                switch(my_valid_data.data[0])
                {
                    case 0xF1:
                    {
                        APP_PRINTF_BUF("valid_data", my_valid_data.data, my_valid_data.length);
                    }
                    break;
                    case 0xF2:
                    {
                        app_save_config(&my_valid_data);
                    }
                    break;
                    default:
                    break;
                }
            }
        }
    }
}

void app_save_config(valid_data* boj)
{
    APP_PRINTF_BUF("data:", boj->data, boj->length);

    uint32_t output[32] = {0};    
    if(app_uint8_to_uint32(boj->data, boj->length, output, sizeof(output)) == true)
    {
        if(app_flash_program(CONFIG_START_ADDR, output, sizeof(output), true) == FMC_READY)
        {
            if(app_load_config() != true)
            {
                APP_PRINTF("app_load_config error\n");
            }
        }
    }
}
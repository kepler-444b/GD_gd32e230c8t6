#include "gd32e23x.h"
#include <stdio.h>
#include "panel_base.h"
#include "../base/base.h"
#include "../uart/uart.h"
#include "../base/debug.h"


uint8_t calcrc_data(uint8_t *rxbuf ,uint8_t len);
void app_at_send(uint8_t *data, uint16_t len);
void app_panel_send_cmd(uint8_t key_number, uint8_t cmd)
{
    uint8_t frame[10] = {0};
    frame[0] = 0XF1;
    frame[1] = my_dev_config.func[key_number];
    frame[2] = cmd;
    frame[3] = my_dev_config.group[key_number];
    frame[4] = my_dev_config.area[key_number];
    frame[5] = calcrc_data(frame,6);
    frame[6] = my_dev_config.perm[key_number];
    frame[7] = my_dev_config.scene_group[key_number];
    app_at_send(frame, 8);
    APP_PRINTF_BUF("send:", frame, 8);
}

uint8_t calcrc_data(uint8_t *rxbuf ,uint8_t len)
{
  uint8_t i,sum=0;
  for(i=0;i<len;i++)sum = sum+rxbuf[i];
  return (0xff-sum+1);
}

void app_at_send(uint8_t *data, uint16_t len) 
{   
    // 转换为十六进制字符串
    char hex_buffer[125] = {0};
    char at_frame[256] = {0};
    for(int i=0; i < len; i++) 
    {
        snprintf(hex_buffer + 2 * i, 3, "%02X", data[i]);
    }
    
    snprintf(at_frame, sizeof(at_frame), "AT+SEND=FFFFFFFFFFFF,%d,\"%s\",1\r\n", len * 2, hex_buffer);
    APP_PRINTF("at_frame:%s\r\n", at_frame);
    app_usart0_send_string(at_frame);
    
}

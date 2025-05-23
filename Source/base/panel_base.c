#include "gd32e23x.h"
#include <stdio.h>
#include "panel_base.h"
#include "../base/base.h"
#include "../uart/uart.h"
#include "../base/debug.h"
#include "../protocol/protocol.h"

#if 0
// 函数声明
uint8_t calcrc_data(uint8_t *rxbuf ,uint8_t len);

// 构造帧
void app_panel_send_cmd(uint8_t key_number, uint8_t key_status, uint8_t cmd)
{
    uint8_t frame[64] = {0};
    switch(cmd)
    {
        case 0xF1:
        {   // 发送给通信帧
            frame[0] = 0XF1;
            frame[1] = my_dev_config.func[key_number];
            frame[2] = key_status;
            frame[3] = my_dev_config.group[key_number];
            frame[4] = my_dev_config.area[key_number];
            frame[5] = calcrc_data(frame,6);
            frame[6] = my_dev_config.perm[key_number];
            frame[7] = my_dev_config.scene_group[key_number];

            app_at_send(frame, 8);
        }
        break;
        case 0xF8:
        {   // 进入设置模式
            frame[0] = 0XF8;
            frame[1] = 0x01;
            frame[2] = 0x07;
            app_at_send(frame, 3);
        }
    }
    
}

uint8_t calcrc_data(uint8_t *rxbuf ,uint8_t len)
{
  uint8_t i,sum=0;
  for(i=0;i<len;i++)sum = sum+rxbuf[i];
  return (0xff-sum+1);
}
#endif


#ifndef _PLCP_OTA_H_
#define _PLCP_OTA_H_

#include "plcp_panel.h"

uint8_t PLCP_ota_updata(uint8_t* updata_msg, uint16_t msg_len);

uint8_t PLCP_ota_file(uint8_t* file_msg, uint16_t msg_len);

uint16_t PLCP_ota_res(uint8_t* ack);

#endif


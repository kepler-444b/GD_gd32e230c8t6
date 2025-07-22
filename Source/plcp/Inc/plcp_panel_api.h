
#ifndef _PLCP_API_H_
#define _PLCP_API_H_

#include "plcp_panel.h"

#if 0
void plcp_panel_init(uint8_t button_num_max, uint8_t relay_num_max);
void plcp_panel_main_loop_process(void);

void plcp_panel_event_notify(uint8_t button_id);
uint8_t plcp_panel_uart_recv_process(uint8_t *recbuf, uint16_t reclen);

void plcp_panel_factory(void);
void plcp_panel_reset(void);
uint16_t plcp_panel_get_did(void);
#endif
#endif

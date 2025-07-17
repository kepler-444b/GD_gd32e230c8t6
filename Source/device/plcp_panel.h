#ifndef _PLCP_PANEL_H_
#define _PLCP_PANEL_H_
#include "gd32e23x.h"
#include <stdbool.h>

void plcp_panel_key_init(void);
bool plcp_panel_set_status(char *aei, uint8_t *stateParam, uint16_t stateParamLen);
#endif

#include <stdio.h>
#include "device_manager.h"
#include "jump_device.h"
void app_jump_device_init(void)
{
#if defined PANEL_KEY6
#include "panel_key6.h"
    panel_key6_init();

#endif
}

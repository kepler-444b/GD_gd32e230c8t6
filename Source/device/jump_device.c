#include <stdio.h>
#include "device_manager.h"
#include "jump_device.h"

#if defined PANEL_KEY
    #if defined PLCP_PANEL_4KEY
        #include "plcp_panel.h"
    #else
        #include "panel_key.h"
    #endif

#endif

#if defined QUICK_BOX
    #include "quick_box.h"
#endif

void app_jump_device_init(void)
{

#if defined PANEL_KEY
    #if defined PLCP_PANEL_4KEY
    plcp_panel_key_init();
    #else
    panel_key_init();
    #endif

#endif

#if defined QUICK_BOX
    quick_box_init();
#endif
}

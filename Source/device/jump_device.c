#include <stdio.h>
#include "device_manager.h"
#include "jump_device.h"

#if defined PANEL_KEY
    #include "panel_key.h"
#endif

#if defined PANEL_PLCP
    #include "plcp_panel.h"
#endif

#if defined QUICK_BOX
    #include "quick_box.h"
#endif

void app_jump_device_init(void)
{
#if defined PANEL_KEY
    panel_key_init();
#endif
#if defined PANEL_PLCP
    plcp_panel_key_init();
#endif
#if defined QUICK_BOX
    quick_box_init();
#endif
}

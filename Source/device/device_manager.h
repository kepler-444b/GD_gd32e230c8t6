#ifndef _DEVICE_MANAGER_H_
#define _DEVICE_MANAGER_H_
#include <stdio.h>
/*
    2025.05.14 舒东升
    在此管理每个设备的初始化
*/

// 设备类型(多选一)
// #define PANEL_KEY // 灯控面板
#define QUICK_BOX // 快装盒子

#if defined PANEL_KEY
    // 面板类型(多选一)
    // #define PANEL_4KEY
    // #define PANEL_6KEY
    #define PANEL_8KEY

    #if defined PANEL_4KEY
        #define KEY_NUMBER 4
    #elif defined PANEL_6KEY
        #define KEY_NUMBER 6
    #elif defined PANEL_8KEY
        #define KEY_NUMBER 4
    #endif
#endif

#if defined QUICK_BOX

    #define LED_NUMBER 6
    #define PWM_DIR    // pwm 方向(开启为反,关闭为正)

#endif

#endif //_DEVICE_MANAGER_H_
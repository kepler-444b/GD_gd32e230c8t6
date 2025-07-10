#ifndef _DEVICE_MANAGER_H_
#define _DEVICE_MANAGER_H_
#include <stdio.h>
/*
    2025.05.14 舒东升
    在此管理每个设备的初始化
*/

// 设备类型(多选一)
#define PANEL_KEY // 灯控面板
// #define QUICK_BOX_WZR // 快装盒子

#if defined PANEL_KEY

    // 面板类型(多选一)
    /* ************ 微自然 ************ */
    // #define PANEL_4KEY_A11_WZR
    // #define PANEL_6KEY_A11_WZR
    // #define PANEL_8KEY_A13_WZR

    /* ************ 力合微 ************ */
    #define PANEL_4KEY_A13_LHW
    // #define PANEL_6KEY_A13_LHW

    /* *********** 确定继电器数量 *********** */
    #define RELAY_NUMBER 4

    /* ************ 确定按键数量 ************ */
    #if defined PANEL_4KEY_A11_WZR
        #define KEY_NUMBER 4
    #elif defined PANEL_6KEY_A11_WZR
        #define KEY_NUMBER 6
    #elif defined PANEL_8KEY_A13_WZR
        #define KEY_NUMBER 4
    #endif
    #if defined PANEL_4KEY_A13_LHW
        #define KEY_NUMBER 4
    #endif
    #if defined PANEL_6KEY_A13_LHW
        #define KEY_NUMBER 6
    #endif
#endif

#if defined QUICK_BOX_WZR

    #define LED_NUMBER 6
    #define PWM_DIR    // pwm 方向(开启为反,关闭为正)

#endif

#endif //_DEVICE_MANAGER_H_
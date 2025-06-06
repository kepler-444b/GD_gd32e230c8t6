#ifndef _DEVICE_MANAGER_H_
#define _DEVICE_MANAGER_H_
#include <stdio.h>
/*
    2025.05.14 舒东升
    在此管理每个设备的初始化
*/

#define PANEL_KEY // 灯控面板

#if defined PANEL_KEY
// #define PANEL_6KEY
#define PANEL_8KEY

#if defined PANEL_6KEY
#define KEY_NUMBER_COUNT 6
#elif defined PANEL_8KEY
#define KEY_NUMBER_COUNT 8

#endif
#endif

// #define QUICK_BOX  // 快装盒子

#endif
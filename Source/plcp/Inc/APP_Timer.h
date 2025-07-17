/******************************************************************************
 * (c) CRCright 2017-2020, Leaguer Microelectronics, All Rights Reserved
 * THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF Leaguer Microelectronics, INC.
 * The CRCright notice above does not evidence any actual or intended
 * publication of such source code.
 *
 * file: APP_Timer.h
 * author : dingy   dingy@leaguerme.com
 * date   : 2020/04/16
 * version: v1.0
 * description: 本文件是APP层定时器处理模块的头文件声明
 *
 * modify record:
 *****************************************************************************/

/********************************************************************************/
#ifndef _APP_TIMER_H_
#define _APP_TIMER_H_
/*******************************************************************************/

#include "plcp_panel.h"

/*--------------------------------------------------------------
函数名称：APP_Timer_Init
函数功能：APP层定时器初始化
输入参数：无
返回值：无
备 注：
---------------------------------------------------------------*/
void APP_Timer_Init(void);

void APP_TimerRun(void);
void APP_TimerTickCount(void);

void APP_StartGenTimer(uint8_t timerId);
void APP_StopGenTimer(uint8_t timerId);
void APP_SetGenTimer(uint8_t timerId, uint32_t period);
uint8_t APP_NewGenTimer(uint32_t period, void (*timeoutCallback)(void));
uint8_t APP_CheckGenTimerRunning(uint8_t timerId);

/***********************************函数声明 结束***********************************/

#endif

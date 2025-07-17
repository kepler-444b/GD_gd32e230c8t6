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
 * description: ���ļ���APP�㶨ʱ������ģ���ͷ�ļ�����
 *
 * modify record:
 *****************************************************************************/

/********************************************************************************/
#ifndef _APP_TIMER_H_
#define _APP_TIMER_H_
/*******************************************************************************/

#include "plcp_panel.h"

/*--------------------------------------------------------------
�������ƣ�APP_Timer_Init
�������ܣ�APP�㶨ʱ����ʼ��
�����������
����ֵ����
�� ע��
---------------------------------------------------------------*/
void APP_Timer_Init(void);

void APP_TimerRun(void);
void APP_TimerTickCount(void);

void APP_StartGenTimer(uint8_t timerId);
void APP_StopGenTimer(uint8_t timerId);
void APP_SetGenTimer(uint8_t timerId, uint32_t period);
uint8_t APP_NewGenTimer(uint32_t period, void (*timeoutCallback)(void));
uint8_t APP_CheckGenTimerRunning(uint8_t timerId);

/***********************************�������� ����***********************************/

#endif

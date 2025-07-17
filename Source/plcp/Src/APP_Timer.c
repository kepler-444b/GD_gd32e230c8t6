/******************************************************************************
 * (c) CRCright 2017-2020, Leaguer Microelectronics, All Rights Reserved
 * THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF Leaguer Microelectronics, INC.
 * The CRCright notice above does not evidence any actual or intended
 * publication of such source code.
 *
 * file: APP_Timer.c
 * author : dingy   dingy@leaguerme.com
 * date   : 2020/04/16
 * version: v1.0
 * description: 本文件是APP层定时器处理模块的源文件声明
 *
 * modify record:
 *****************************************************************************/

/********************************************************************************/
#include "stdint.h"
#include "string.h"
/*******************************************************************************/

/**********************************宏定义 开始*************************************/
#define MAX_GEN_TIMER	8
/**********************************宏定义 结束*************************************/

/**********************************枚举声明 开始************************************/

/**********************************枚举声明 结束************************************/

/**********************************结构体声明 开始***********************************/
typedef struct {
	uint8_t enable;
	uint32_t period;
    uint32_t counter;
    void (*timeoutCallback)(void);
}APP_GEN_TIMER_STRUCT;
/**********************************结构体声明 结束***********************************/

/**********************************静态变量声明 开始**********************************/
static uint32_t timerTickCounter = 0;
static APP_GEN_TIMER_STRUCT APP_genTimers[MAX_GEN_TIMER];
/**********************************静态变量声明 结束**********************************/

/***********************************函数声明 开始***********************************/

/*--------------------------------------------------------------
函数名称：APP_Timer_Init
函数功能：APP层定时器初始化
输入参数：无
返回值：无
备 注：
---------------------------------------------------------------*/
void APP_Timer_Init(void)
{
	timerTickCounter = 0;
	memset(APP_genTimers, 0, MAX_GEN_TIMER*sizeof(APP_GEN_TIMER_STRUCT));
}

void APP_TimerTickCount(void)
{
	timerTickCounter++;
}

void APP_TimerRun(void)
{
	uint8_t i;
	uint32_t counterTemp = timerTickCounter;
	
	timerTickCounter = 0;

	for(i = 0; i < MAX_GEN_TIMER; i++)
	{
		if (APP_genTimers[i].enable == 1 
			&& APP_genTimers[i].period > 0
			&& APP_genTimers[i].counter > 0
			&& APP_genTimers[i].timeoutCallback != 0)
		{
			if(APP_genTimers[i].counter > APP_genTimers[i].period)
			{
				APP_genTimers[i].counter = APP_genTimers[i].period;
			}
			if(counterTemp < APP_genTimers[i].counter)
			{
				APP_genTimers[i].counter -= counterTemp;
			}
			else
			{
				APP_genTimers[i].counter = 0;
			}

			if (APP_genTimers[i].counter == 0)
			{
				APP_genTimers[i].counter = APP_genTimers[i].period;
				APP_genTimers[i].timeoutCallback();
			}
		}
	}
}

uint8_t APP_NewGenTimer(uint32_t period, void (*callback)(void))
{
	uint8_t i;
	for(i = 0; i < MAX_GEN_TIMER; i++)
	{
		if (APP_genTimers[i].enable == 0
			&& APP_genTimers[i].period == 0
			&& APP_genTimers[i].counter == 0
			&& APP_genTimers[i].timeoutCallback == 0)
		{
			APP_genTimers[i].enable = 1;
			APP_genTimers[i].period = period;
			APP_genTimers[i].counter = 0;
			APP_genTimers[i].timeoutCallback = callback;
			return i;
		}
	}
	return 0xff;
}

void APP_StartGenTimer(uint8_t timerId)
{
	if(timerId >= MAX_GEN_TIMER 
	|| APP_genTimers[timerId].enable == 0
	|| APP_genTimers[timerId].period == 0
	|| APP_genTimers[timerId].counter > 0
	|| APP_genTimers[timerId].timeoutCallback == 0)
	{
		return;
	}
	APP_genTimers[timerId].counter = APP_genTimers[timerId].period;
}


void APP_StopGenTimer(uint8_t timerId)
{
	if(timerId >= MAX_GEN_TIMER
	|| APP_genTimers[timerId].enable == 0
	|| APP_genTimers[timerId].period == 0
	|| APP_genTimers[timerId].counter == 0
	|| APP_genTimers[timerId].timeoutCallback == 0)
	{
		return;
	}
	APP_genTimers[timerId].counter = 0;	
}

void APP_SetGenTimer(uint8_t timerId, uint32_t period)
{
	if(timerId >= MAX_GEN_TIMER 
	|| APP_genTimers[timerId].enable == 0
	|| APP_genTimers[timerId].period == 0
	|| APP_genTimers[timerId].counter > 0
	|| APP_genTimers[timerId].timeoutCallback == 0)
	{
		return;
	}
	APP_genTimers[timerId].period = period;
}

uint8_t APP_CheckGenTimerRunning(uint8_t timerId)
{
	if(timerId >= MAX_GEN_TIMER 
	|| APP_genTimers[timerId].enable == 0
	|| APP_genTimers[timerId].period == 0
	|| APP_genTimers[timerId].timeoutCallback == 0)
	{
		return 0xff;
	}

	if(APP_genTimers[timerId].counter > 0)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

/***********************************函数声明 结束***********************************/

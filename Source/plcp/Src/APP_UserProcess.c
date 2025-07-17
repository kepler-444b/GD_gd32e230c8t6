/********************************************************************************
 * (c) Copyright 2017-2020, LME, All Rights Reserved锟斤拷
 * THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF LME, INC锟斤拷
 * The copyright notice above does not evidence any actual or intended
 * publication of such source code
 *
 * FileName    : APP_UserProcess.h
 * Author      :
 * Date        : 2024-09-06
 * Version     : 1.0
 * Description : 用户实现功能填写内容
 * Others      :
 * ModifyRecord:
 *
 ********************************************************************************/

#include "APP_timer.h"
#include "APP_RxBuffer.h"
#include "APP_TxBuffer.h"

void plcp_panel_uart_send_process(uint8_t* send_data, uint16_t len);

void APP_UappsMsgProcessing(uint8_t *inputDataBuff, uint16_t inputDataBuffLen);

/*****************************************************************************
函数名称 : MCU_UartReceive
功能描述 : MCU串口接收处理数据函数
输入参数 : recbuf:数据源数据；reclen:数据长度
返回参数 : 无
使用说明 : 将
*****************************************************************************/
uint8_t MCU_UartReceive(uint8_t *recbuf, uint16_t reclen)
{
    RxDataParametersStruct rxDataParam;
    rxDataParam.rxData = recbuf;
    rxDataParam.rxDataLen = reclen;
    return APP_RxBuffer_Add(&rxDataParam);
}

static void MCU_Send_date_timer_handler(void)
{
	APP_DataTxTask txTask;
	if (APP_TxBuffer_GetFirstMsgDataParameters(&txTask))
	{
		// printf("sdk uart send\n");
		// printData(txTask.nsdu, txTask.nsduLength);
		plcp_panel_uart_send_process(txTask.nsdu, txTask.nsduLength);
		APP_TxBuffer_DeleteFirstMsg();
	}
}

/*****************************************************************************
函数名称 : MCU_Send_date
功能描述 : 将需要MCU串口发送数据添加到发送队列中
输入参数 : SendBuff:数据源数据; SendBuffLen:数据长度
返回参数 : 无
使用说明 : 在此实现将待发送添加到发送队列中
*****************************************************************************/
void MCU_Send_date(uint8_t *SendBuff, uint16_t SendBuffLen)
{
	static uint8_t MCU_Send_date_timer = 0xff;

    APP_DataTxTask txTask;
    txTask.nsdu = SendBuff;
    txTask.nsduLength = SendBuffLen;
    APP_TxBuffer_Add(&txTask);

	if(MCU_Send_date_timer == 0xff){
		MCU_Send_date_timer = APP_NewGenTimer(25, MCU_Send_date_timer_handler);
		APP_StartGenTimer(MCU_Send_date_timer);
	}
}

/*--------------------------------7-----------------------------
函数名称：APP_Queue_ListenAndHandleMessage
函数功能：APP层消息的监听（接收队列、发送队列）和处理函数。
输入参数：无
返回值：无
备 注：该函数需要在主循环中调用
---------------------------------------------------------------*/
void APP_Queue_ListenAndHandleMessage(void)
{
    RxDataParametersStruct rxDataPointer;
    uint16_t rxBufferLeftAmount = 0;

    rxBufferLeftAmount = APP_RxBuffer_getRxBufferMsgAmount();
    if (rxBufferLeftAmount > 0)
    {
        // printf("rx lostQueueMsg=%d\n", (rxBufferLeftAmount - 1));
        APP_RxBuffer_GetFirstMsgDataParameters(&rxDataPointer);
        // printData(rxDataPointer.rxData, rxDataPointer.rxDataLen);
        APP_UappsMsgProcessing(rxDataPointer.rxData, rxDataPointer.rxDataLen);
        APP_RxBuffer_DeleteFirstMsg();
    }
}

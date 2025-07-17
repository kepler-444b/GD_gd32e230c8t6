/********************************************************************************
 * (c) Copyright 2017-2020, LME, All Rights Reserved。
 * THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF LME, INC。
 * The copyright notice above does not evidence any actual or intended
 * publication of such source code。
 *
 * FileName    : APP_RxBuffer.c
 * Author      : dingy
 * Date        : 2020-04-17
 * Version     : 1.0
 * Description : APP层的接收缓存模块的源文件
 *             :
 * Others      :
 * ModifyRecord:
 *
********************************************************************************/
#include "APP_RxBuffer.h"
/*******************************************************************************/

/**********************************静态变量声明 开始**********************************/
static uint8_t  APP_RxBuffer[APP_RX_BUFFER_SIZE];
static RxDataParametersStruct APP_RxBufferDataParamArray[APP_RX_BUFFER_DATA_PARAM_ARRAY_SIZE];
static uint16_t APP_RxBufferIndex = 0;
static uint16_t APP_RxBufferDataParamArrayIndex = 0;

/**********************************静态变量声明 结束**********************************/

/*-------------------------------------------------------------
函数名称：APP_RxBuffer_init
函数功能：APP层接收缓存的初始化。
输入参数：无
返回值：无
备 注：
---------------------------------------------------------------*/
void APP_RxBuffer_init(void)
{
    memset(APP_RxBuffer, 0, sizeof(APP_RxBuffer));
    memset(APP_RxBufferDataParamArray, 0, sizeof(APP_RxBufferDataParamArray));
    APP_RxBufferIndex = 0;
    APP_RxBufferDataParamArrayIndex = 0;
}

/*-------------------------------------------------------------
函数名称：APP_RxBuffer_IsEmpty
函数功能：APP层接收缓存是否空。
输入参数：无
返回值：0-否，1-是
备 注：
---------------------------------------------------------------*/
static uint8_t APP_RxBuffer_IsEmpty(void)
{
    if (APP_RxBufferIndex == 0 && APP_RxBufferDataParamArrayIndex == 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/*-------------------------------------------------------------
函数名称：APP_RxBuffer_Add
函数功能：APP层接收缓存添加一个元素。
输入参数：inputDataPointer -- 添加数据的指针
        inputDataLen -- 添加数据的长度
返回值： 0-添加失败，1-添加成功
备 注：
---------------------------------------------------------------*/
uint8_t APP_RxBuffer_Add(RxDataParametersStruct *param)
{
    if ((APP_RxBufferIndex + param->rxDataLen) > APP_RX_BUFFER_SIZE)
    {
        return 0;
    }
    
    if (APP_RxBufferDataParamArrayIndex > APP_RX_BUFFER_DATA_PARAM_ARRAY_SIZE)
    {
        return 0;
    }
    memcpy(&APP_RxBuffer[APP_RxBufferIndex], param->rxData, param->rxDataLen);
    APP_RxBufferDataParamArray[APP_RxBufferDataParamArrayIndex].rxDataLen = param->rxDataLen;
    APP_RxBufferDataParamArray[APP_RxBufferDataParamArrayIndex].rxData = &APP_RxBuffer[APP_RxBufferIndex];

    APP_RxBufferIndex += APP_RxBufferDataParamArray[APP_RxBufferDataParamArrayIndex].rxDataLen;
    APP_RxBufferDataParamArrayIndex++;
    return 1;
}

/*-------------------------------------------------------------
函数名称：APP_RxBuffer_DeleteFirstMsg
函数功能：APP层接收缓存删除第一条消息。
输入参数：deleteDataLen -- 删除数据的长度
返回值：  0-删除失败，1-删除成功
备 注：
---------------------------------------------------------------*/
uint8_t APP_RxBuffer_DeleteFirstMsg(void)
{
    uint16_t firstMsgLen = 0;
    if (APP_RxBuffer_IsEmpty())
    {
        return 0;
    }

    firstMsgLen = APP_RxBufferDataParamArray[0].rxDataLen;

    memcpy(APP_RxBuffer, &(APP_RxBuffer[firstMsgLen]), (APP_RX_BUFFER_SIZE - firstMsgLen)*sizeof(uint8_t));
    APP_RxBufferIndex -= firstMsgLen;

    memcpy(APP_RxBufferDataParamArray, &(APP_RxBufferDataParamArray[1]), (APP_RX_BUFFER_DATA_PARAM_ARRAY_SIZE - 1)*sizeof(RxDataParametersStruct));
    APP_RxBufferDataParamArrayIndex--;

    return 1;
}

/*-------------------------------------------------------------
函数名称：APP_RxBuffer_GetFirstMsgDataParameters
函数功能：APP层接收缓存获取第一条消息的参数
输出参数：firstMsgDataParam -  接收缓存第一条消息的参数
返回值： 0-失败，1-成功
备 注：
---------------------------------------------------------------*/
uint8_t APP_RxBuffer_GetFirstMsgDataParameters(RxDataParametersStruct *firstMsgDataParam)
{
//  uint16_t firstMsgLen = 0;

    if (APP_RxBuffer_IsEmpty())
    {
        return 0;
    }

    if (APP_RxBufferDataParamArray[0].rxDataLen == 0)
    {
        return 0;
    }
    firstMsgDataParam->rxDataLen = APP_RxBufferDataParamArray[0].rxDataLen;
    firstMsgDataParam->rxData = APP_RxBuffer;

    return 1;
}

/*----------------------------11----------------------------------
函数名称：APP_RxBuffer_getRxBufferMsgAmount
函数功能：获取rxBuffer中的信息数量。
输入参数：无
返回值：无
备 注：
---------------------------------------------------------------*/
uint16_t APP_RxBuffer_getRxBufferMsgAmount(void)
{
    return APP_RxBufferDataParamArrayIndex;
}

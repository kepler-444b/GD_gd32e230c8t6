/********************************************************************************
 * (c) Copyright 2017-2020, LME, All Rights Reserved。
 * THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF LME, INC。
 * The copyright notice above does not evidence any actual or intended
 * publication of such source code。
 *
 * FileName    : APP_TxBuffer.c
 * Author      : dingy
 * Date        : 2020-04-17
 * Version     : 1.0
 * Description : APP层的发送缓存模块的源文件
 *             :
 * Others      :
 * ModifyRecord:
 *
 ********************************************************************************/

#include "APP_TxBuffer.h"

/*******************************************************************************/

/**********************************宏定义 开始*************************************/

/**********************************宏定义 结束*************************************/

/**********************************枚举声明 开始************************************/

/**********************************枚举声明 结束************************************/

/**********************************结构体声明 开始***********************************/

/**********************************结构体声明 结束***********************************/

/**********************************静态变量声明 开始**********************************/
static uint8_t APP_TxBuffer[APP_TX_BUFFER_DATA_PARAM_ARRAY_SIZE][APP_TX_BUFFER_SIZE];
static APP_DataTxTask APP_TxBufferDataParamArray[APP_TX_BUFFER_DATA_PARAM_ARRAY_SIZE];
static uint16_t APP_TxBufferDataParamArrayIndex = 0;

/**********************************静态变量声明 结束**********************************/

/**********************************函数列表  开始************************************
 * 1、     void APP_TxBuffer_init()
 * 2、     uint8_t APP_TxBuffer_Add(uint8_t* inputDataPointer, APP_DataTxTask* inputDataParam)
 * 3、     uint8_t APP_TxBuffer_DeleteFirstMsg()
 * 4、     uint16_t APP_TxBuffer_GetFirstMsg(uint16_t outputDataMaxLen, uint8_t* outputData)
 * 5、     uint8_t APP_TxBuffer_IsFull()
 * 6、     uint8_t APP_TxBuffer_IsEmpty()
 * 7、     uint8_t APP_TxBuffer_CanAddNewMsg(uint16_t inputDataLen)

***********************************函数列表  结束***********************************/

/***********************************函数声明 开始***********************************/

/*--------------------------------1-----------------------------
函数名称：APP_TxBuffer_init
函数功能：APP层发送缓存的初始化。
输入参数：无
返回值：无
备 注：
---------------------------------------------------------------*/
void APP_TxBuffer_init(void)
{
    memset(APP_TxBuffer, 0, sizeof(APP_TxBuffer));
    memset(APP_TxBufferDataParamArray, 0, sizeof(APP_TxBufferDataParamArray));
    APP_TxBufferDataParamArrayIndex = 0;
}

/*--------------------------------------------------------------
函数名称：APP_TxBuffer_Add2FirstMsg
函数功能：APP层发送缓存添加一个元素，并且排到首个元素的位置。
输入参数：inputDataPointer -- 添加数据的指针
        inputDataLen -- 添加数据的长度
返回值： 0-添加失败，1-添加成功
备 注：
---------------------------------------------------------------*/
uint8_t APP_TxBuffer_Add2FirstMsg(APP_DataTxTask *inputDataParam)
{
    if (1 == APP_TxBuffer_Add(inputDataParam))
    {
        if (APP_TxBufferDataParamArrayIndex > 1)
        {
            uint8_t APP_TxBuffer_tmp[APP_TX_BUFFER_SIZE];
            APP_DataTxTask txTmp;

            for (uint8_t i = 0; i < APP_TxBufferDataParamArrayIndex - 1; i++)
            {
                memcpy(APP_TxBuffer_tmp, &APP_TxBuffer[0], sizeof(APP_TxBuffer_tmp));
                memcpy(&txTmp, &APP_TxBufferDataParamArray[0], sizeof(txTmp));
                APP_TxBuffer_DeleteFirstMsg();
                APP_TxBuffer_Add(&txTmp);
            }
        }
        return 1;
    }
    else
    {
        return 0;
    }
}

/*--------------------------------2-----------------------------
函数名称：APP_TxBuffer_Add
函数功能：APP层发送缓存添加一个元素。
输入参数：inputDataPointer -- 添加数据的指针
        inputDataLen -- 添加数据的长度
返回值： 0-添加失败，1-添加成功
备 注：
---------------------------------------------------------------*/
uint8_t APP_TxBuffer_Add(APP_DataTxTask *inputDataParam)
{
    static uint16_t addFailCount = 0;

    if (addFailCount > 3)
    {
        addFailCount = 0;
        APP_TxBuffer_init();
        // printf( "tx full reset\n");
    }

    if (inputDataParam->nsduLength > APP_TX_BUFFER_SIZE)
    {
        addFailCount++;
        return 0;
    }

    if (APP_TxBufferDataParamArrayIndex >= APP_TX_BUFFER_DATA_PARAM_ARRAY_SIZE)
    {
        addFailCount++;
        return 0;
    }

    addFailCount = 0;
    memcpy(&(APP_TxBuffer[APP_TxBufferDataParamArrayIndex]), inputDataParam->nsdu, inputDataParam->nsduLength * sizeof(uint8_t));
    memcpy(&(APP_TxBufferDataParamArray[APP_TxBufferDataParamArrayIndex]), inputDataParam, sizeof(APP_DataTxTask));
    APP_TxBufferDataParamArray[APP_TxBufferDataParamArrayIndex].nsdu = &(APP_TxBuffer[APP_TxBufferDataParamArrayIndex][0]);
    APP_TxBufferDataParamArrayIndex++;
    return 1;
}

/*--------------------------------3-----------------------------
函数名称：APP_TxBuffer_DeleteFirstMsg
函数功能：APP层发送缓存删除第一条消息。
输入参数：deleteDataLen -- 删除数据的长度
返回值：  0-删除失败，1-删除成功
备 注：
---------------------------------------------------------------*/
uint8_t APP_TxBuffer_DeleteFirstMsg(void)
{

    if (APP_TxBuffer_IsEmpty())
    {
        return 0;
    }

    memcpy(&(APP_TxBuffer[0]), &(APP_TxBuffer[1]), (APP_TX_BUFFER_DATA_PARAM_ARRAY_SIZE - 1) * APP_TX_BUFFER_SIZE * sizeof(uint8_t));
    memcpy(APP_TxBufferDataParamArray, &(APP_TxBufferDataParamArray[1]), (APP_TX_BUFFER_DATA_PARAM_ARRAY_SIZE - 1) * sizeof(APP_DataTxTask));
    for (uint8_t i = 0; i < APP_TxBufferDataParamArrayIndex; i++)
    {
        APP_TxBufferDataParamArray[i].nsdu = &(APP_TxBuffer[i][0]);
    }
    APP_TxBufferDataParamArrayIndex--;

    return 1;
}

/*--------------------------------4-----------------------------
函数名称：APP_TxBuffer_GetFirstMsgDataParameters
函数功能：APP层发送缓存获取第一条消息的参数
输出参数：firstMsgDataParam -  发送缓存第一条消息的参数
返回值： 0-失败，1-成功
备 注：
---------------------------------------------------------------*/
uint8_t APP_TxBuffer_GetFirstMsgDataParameters(APP_DataTxTask *firstMsgDataParam)
{

    if (APP_TxBuffer_IsEmpty())
    {
        return 0;
    }
    memcpy((uint8_t *)firstMsgDataParam, (uint8_t *)&APP_TxBufferDataParamArray[0], sizeof(APP_DataTxTask));

    return 1;
}

/*--------------------------------5-----------------------------
函数名称：APP_TxBuffer_GetFirstMsgPointer
函数功能：APP层发送缓存获取第一条消息的消息缓存指针
输入参数：无
返回值：APP层发送缓存获取第一条消息的消息缓存指针
备 注：
---------------------------------------------------------------*/
uint8_t *APP_TxBuffer_GetFirstMsgPointer(void)
{
    return APP_TxBuffer[0];
}

/*--------------------------------6-----------------------------
函数名称：APP_TxBuffer_IsFull
函数功能：APP层发射缓存是否满。
输入参数：无
返回值：0-否，1-是
备 注：
---------------------------------------------------------------*/
uint8_t APP_TxBuffer_IsFull(void)
{
    if (APP_TxBufferDataParamArrayIndex >= APP_TX_BUFFER_DATA_PARAM_ARRAY_SIZE)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/*--------------------------------7-----------------------------
函数名称：APP_TxBuffer_IsEmpty
函数功能：APP层发送缓存是否空。
输入参数：无
返回值：0-否，1-是
备 注：
---------------------------------------------------------------*/
uint8_t APP_TxBuffer_IsEmpty(void)
{
    if (APP_TxBufferDataParamArrayIndex == 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/*--------------------------------8-----------------------------
函数名称：APP_TxBuffer_CanAddNewMsg
函数功能：APP层发送缓存是否添加新消息。
输入参数：inputDataLen -- 输入数据的长度
返回值：0-否，1-是
备 注：
---------------------------------------------------------------*/
uint8_t APP_TxBuffer_CanAddNewMsg(void)
{

    if (APP_TxBufferDataParamArrayIndex >= APP_TX_BUFFER_DATA_PARAM_ARRAY_SIZE)
    {
        return 0;
    }

    return 1;
}

/*--------------------------------9-----------------------------
函数名称：APP_TxBuffer_GetTxTaskNum
函数功能：APP层获取TxBuffer
输入参数：无
返回值：当前索引值
备 注：
---------------------------------------------------------------*/
uint16_t APP_TxBuffer_GetTxTaskNum(void)
{
    return APP_TxBufferDataParamArrayIndex;
}

/********************************************************************************
 * (c) Copyright 2017-2020, LME, All Rights Reserved。
 * THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF LME, INC。
 * The copyright notice above does not evidence any actual or intended
 * publication of such source code。
 *
 * FileName    : APP_TxBuffer.h
 * Author      : dingy
 * Date        : 2020-04-17
 * Version     : 1.0
 * Description : APP层的发射缓存模块的头文件
 *             :
 * Others      :
 * ModifyRecord:
 *
 ********************************************************************************/
#ifndef _APP_TX_BUFFER_H_
#define _APP_TX_BUFFER_H_

#include "plcp_panel.h"

/*******************************************************************************/

/**********************************宏定义 开始*************************************/
#define aMaxNSDUSize 450     // NSDU数据的最大长度 aMaxNSDUSize
#define APP_TX_BUFFER_SIZE 256 //aMaxNSDUSize		  // APP层发射缓存的大小
#define APP_TX_BUFFER_DATA_PARAM_ARRAY_SIZE 4 // APP层发射缓存中发送数据参数队列的大小
/**********************************宏定义 结束*************************************/

/**********************************枚举声明 开始************************************/

/**********************************枚举声明 结束************************************/

/**********************************结构体声明 开始***********************************/
typedef struct
{
	uint16_t nsduLength;			   // 数据长度
	uint8_t *nsdu;				   // 数据内容
} APP_DataTxTask;

/**********************************结构体声明 结束***********************************/

/**********************************静态变量声明 开始**********************************/

/**********************************静态变量声明 结束**********************************/

/**********************************函数列表  开始************************************
 * 1、     void APP_TxBuffer_init()
 * 2、     uint8_t APP_TxBuffer_Add(uint8_t* inputDataPointer, uint16_t inputDataLen)
 * 3、     uint8_t APP_TxBuffer_DeleteFirstMsg()
 * 4、     uint16_t APP_TxBuffer_GetFirstMsg(uint16_t outputDataMaxLen, uint8_t* outputData)
 * 5、     uint8_t APP_TxBuffer_IsFull()
 * 6、     uint8_t APP_TxBuffer_IsEmpty()
 * 7、     uint8_t APP_TxBuffer_CanAddNewMsg(uint16_t inputDataLen)

***********************************函数列表  结束***********************************/

/***********************************函数声明 开始***********************************/

/*--------------------------------1-----------------------------
函数名称：APP_TxBuffer_init
函数功能：APP层发射缓存的初始化。
输入参数：无
返回值：无
备 注：
---------------------------------------------------------------*/
void APP_TxBuffer_init(void);

/*--------------------------------------------------------------
函数名称：APP_TxBuffer_Add2FirstMsg
函数功能：APP层发送缓存添加一个元素，并且排到首个元素的位置。
输入参数：inputDataPointer -- 添加数据的指针
		inputDataLen -- 添加数据的长度
返回值： 0-添加失败，1-添加成功
备 注：
---------------------------------------------------------------*/
uint8_t APP_TxBuffer_Add2FirstMsg(APP_DataTxTask *inputDataParam);

/*--------------------------------2-----------------------------
函数名称：APP_TxBuffer_Add
函数功能：APP层发射缓存添加一个元素。
输入参数：inputDataPointer -- 添加数据的指针
		inputDataLen -- 添加数据的长度
返回值： 0-添加失败，1-添加成功
备 注：
---------------------------------------------------------------*/
uint8_t APP_TxBuffer_Add(APP_DataTxTask *inputDataParam);

/*--------------------------------3-----------------------------
函数名称：APP_TxBuffer_DeleteFirstMsg
函数功能：APP层发射缓存删除第一条消息。
输入参数：deleteDataLen -- 删除数据的长度
返回值：  0-删除失败，1-删除成功
备 注：
---------------------------------------------------------------*/
uint8_t APP_TxBuffer_DeleteFirstMsg(void);

/*--------------------------------4-----------------------------
函数名称：APP_TxBuffer_GetFirstMsgDataParameters
函数功能：APP层发射缓存获取第一条消息的参数
输出参数：firstMsgDataParam -  发射缓存第一条消息的参数
返回值： 0-失败，1-成功
备 注：
---------------------------------------------------------------*/
uint8_t APP_TxBuffer_GetFirstMsgDataParameters(APP_DataTxTask *firstMsgDataParam);

/*--------------------------------5-----------------------------
函数名称：APP_TxBuffer_GetFirstMsgPointer
函数功能：APP层发射缓存获取第一条消息的消息缓存指针
输入参数：无
返回值：APP层发射缓存获取第一条消息的消息缓存指针
备 注：
---------------------------------------------------------------*/
uint8_t *APP_TxBuffer_GetFirstMsgPointer(void);

/*--------------------------------6-----------------------------
函数名称：APP_TxBuffer_IsFull
函数功能：APP层发射缓存是否满。
输入参数：无
返回值：0-否，1-是
备 注：
---------------------------------------------------------------*/
uint8_t APP_TxBuffer_IsFull(void);

/*--------------------------------7-----------------------------
函数名称：APP_TxBuffer_IsEmpty
函数功能：APP层发射缓存是否空。
输入参数：无
返回值：0-否，1-是
备 注：
---------------------------------------------------------------*/
uint8_t APP_TxBuffer_IsEmpty(void);

/*--------------------------------8-----------------------------
函数名称：APP_TxBuffer_CanAddNewMsg
函数功能：APP层发射缓存是否添加新消息。
输入参数：inputDataLen -- 输入数据的长度
返回值：0-否，1-是
备 注：
---------------------------------------------------------------*/
uint8_t APP_TxBuffer_CanAddNewMsg(void);

/*--------------------------------9-----------------------------
函数名称：APP_TxBuffer_GetTxTaskNum
函数功能：APP层获取TxBuffer
输入参数：无
返回值：当前索引值
备 注：
---------------------------------------------------------------*/
uint16_t APP_TxBuffer_GetTxTaskNum(void);

#endif

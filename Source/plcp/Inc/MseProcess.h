/********************************************************************************
 * (c) Copyright 2017-2020, LME, All Rights Reserved��
 * THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF LME, INC��
 * The copyright notice above does not evidence any actual or intended
 * publication of such source code��
 *
 * FileName    : Common.h
 * Author      :
 * Date        : 2023-09-13
 * Version     : 1.0
 * Description :
 *             :
 * Others      :
 * ModifyRecord:
 *
 ********************************************************************************/
#ifndef __MSEPROCESS_H_
#define __MSEPROCESS_H_

#include "Uapps.h"

/**********************************������� ��ʼ***********************************/

typedef enum
{
    MSE_GET_DID = 0x23,
	MSE_SET_Factory,
	MSE_SET_RST,
	MSE_GET_INIT_INFO,
}etoken;

uint8_t APP_SendRSL(char *rslStr, char *from, uint8_t *playload, uint8_t playloadLen);
void APP_UappsMsgProcessing(uint8_t *inputDataBuff, uint16_t inputDataBuffLen);
/***********************************�������� ����************************************/

#endif

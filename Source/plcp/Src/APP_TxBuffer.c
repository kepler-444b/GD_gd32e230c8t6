/********************************************************************************
 * (c) Copyright 2017-2020, LME, All Rights Reserved��
 * THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF LME, INC��
 * The copyright notice above does not evidence any actual or intended
 * publication of such source code��
 *
 * FileName    : APP_TxBuffer.c
 * Author      : dingy
 * Date        : 2020-04-17
 * Version     : 1.0
 * Description : APP��ķ��ͻ���ģ���Դ�ļ�
 *             :
 * Others      :
 * ModifyRecord:
 *
 ********************************************************************************/

#include "APP_TxBuffer.h"

/*******************************************************************************/

/**********************************�궨�� ��ʼ*************************************/

/**********************************�궨�� ����*************************************/

/**********************************ö������ ��ʼ************************************/

/**********************************ö������ ����************************************/

/**********************************�ṹ������ ��ʼ***********************************/

/**********************************�ṹ������ ����***********************************/

/**********************************��̬�������� ��ʼ**********************************/
static uint8_t APP_TxBuffer[APP_TX_BUFFER_DATA_PARAM_ARRAY_SIZE][APP_TX_BUFFER_SIZE];
static APP_DataTxTask APP_TxBufferDataParamArray[APP_TX_BUFFER_DATA_PARAM_ARRAY_SIZE];
static uint16_t APP_TxBufferDataParamArrayIndex = 0;

/**********************************��̬�������� ����**********************************/

/**********************************�����б�  ��ʼ************************************
 * 1��     void APP_TxBuffer_init()
 * 2��     uint8_t APP_TxBuffer_Add(uint8_t* inputDataPointer, APP_DataTxTask* inputDataParam)
 * 3��     uint8_t APP_TxBuffer_DeleteFirstMsg()
 * 4��     uint16_t APP_TxBuffer_GetFirstMsg(uint16_t outputDataMaxLen, uint8_t* outputData)
 * 5��     uint8_t APP_TxBuffer_IsFull()
 * 6��     uint8_t APP_TxBuffer_IsEmpty()
 * 7��     uint8_t APP_TxBuffer_CanAddNewMsg(uint16_t inputDataLen)

***********************************�����б�  ����***********************************/

/***********************************�������� ��ʼ***********************************/

/*--------------------------------1-----------------------------
�������ƣ�APP_TxBuffer_init
�������ܣ�APP�㷢�ͻ���ĳ�ʼ����
�����������
����ֵ����
�� ע��
---------------------------------------------------------------*/
void APP_TxBuffer_init(void)
{
    memset(APP_TxBuffer, 0, sizeof(APP_TxBuffer));
    memset(APP_TxBufferDataParamArray, 0, sizeof(APP_TxBufferDataParamArray));
    APP_TxBufferDataParamArrayIndex = 0;
}

/*--------------------------------------------------------------
�������ƣ�APP_TxBuffer_Add2FirstMsg
�������ܣ�APP�㷢�ͻ������һ��Ԫ�أ������ŵ��׸�Ԫ�ص�λ�á�
���������inputDataPointer -- ������ݵ�ָ��
        inputDataLen -- ������ݵĳ���
����ֵ�� 0-���ʧ�ܣ�1-��ӳɹ�
�� ע��
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
�������ƣ�APP_TxBuffer_Add
�������ܣ�APP�㷢�ͻ������һ��Ԫ�ء�
���������inputDataPointer -- ������ݵ�ָ��
        inputDataLen -- ������ݵĳ���
����ֵ�� 0-���ʧ�ܣ�1-��ӳɹ�
�� ע��
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
�������ƣ�APP_TxBuffer_DeleteFirstMsg
�������ܣ�APP�㷢�ͻ���ɾ����һ����Ϣ��
���������deleteDataLen -- ɾ�����ݵĳ���
����ֵ��  0-ɾ��ʧ�ܣ�1-ɾ���ɹ�
�� ע��
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
�������ƣ�APP_TxBuffer_GetFirstMsgDataParameters
�������ܣ�APP�㷢�ͻ����ȡ��һ����Ϣ�Ĳ���
���������firstMsgDataParam -  ���ͻ����һ����Ϣ�Ĳ���
����ֵ�� 0-ʧ�ܣ�1-�ɹ�
�� ע��
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
�������ƣ�APP_TxBuffer_GetFirstMsgPointer
�������ܣ�APP�㷢�ͻ����ȡ��һ����Ϣ����Ϣ����ָ��
�����������
����ֵ��APP�㷢�ͻ����ȡ��һ����Ϣ����Ϣ����ָ��
�� ע��
---------------------------------------------------------------*/
uint8_t *APP_TxBuffer_GetFirstMsgPointer(void)
{
    return APP_TxBuffer[0];
}

/*--------------------------------6-----------------------------
�������ƣ�APP_TxBuffer_IsFull
�������ܣ�APP�㷢�仺���Ƿ�����
�����������
����ֵ��0-��1-��
�� ע��
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
�������ƣ�APP_TxBuffer_IsEmpty
�������ܣ�APP�㷢�ͻ����Ƿ�ա�
�����������
����ֵ��0-��1-��
�� ע��
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
�������ƣ�APP_TxBuffer_CanAddNewMsg
�������ܣ�APP�㷢�ͻ����Ƿ��������Ϣ��
���������inputDataLen -- �������ݵĳ���
����ֵ��0-��1-��
�� ע��
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
�������ƣ�APP_TxBuffer_GetTxTaskNum
�������ܣ�APP���ȡTxBuffer
�����������
����ֵ����ǰ����ֵ
�� ע��
---------------------------------------------------------------*/
uint16_t APP_TxBuffer_GetTxTaskNum(void)
{
    return APP_TxBufferDataParamArrayIndex;
}

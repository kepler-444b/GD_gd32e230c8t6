/******************************************************************************
 * (c) CRCright 2017-2018, Leaguer Microelectronics, All Rights Reserved
 * THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF Leaguer Microelectronics, INC.
 * The CRCright notice above does not evidence any actual or intended
 * publication of such source code.
 *
 * file: MseProcess.c
 * author :
 * date   : 2023/09/24
 * version: v1.0
 * description: ���ļ����յ����ݺ���н����ж��Ƿ�ΪUapps�����ٽ��зְ�����ο�����
 *
 * modify record:��Ҫ���н���Uapps���Ĳ����ݶ�Ӧ�Ĺ��ܽ��д�����Ӧ
 *****************************************************************************/
#include "MseProcess.h"
#include "PLCP_ota.h"
#include "../usart/usart.h"
#include "gd32e23x.h"
#include "../base/debug.h"
#include "../device/plcp_panel.h"

#define MSE_DEVICE_SET_STATE "/_state" // ����״̬

/*OTA*/
#define MSE_OTA_UPDATE "/mcu/update"
#define MSE_OTA_FILE   "/mcu/file"
#define MSE_OTA_RES    "/mcu/res"

void MCU_Send_date(uint8_t *SendBuff, uint16_t SendBuffLen);

void plcp_panel_led_process(uint8_t id, uint8_t onoff);
void plcp_panel_bk_process(uint8_t id, uint8_t onoff);
void plcp_panel_realy_process(uint8_t id, uint8_t onoff);

extern uint8_t sycn_flag;

uint8_t PLCP_write_state(char *aei, uint8_t *stateParam, uint16_t stateParamLen)
{
    APP_PRINTF_BUF("recv", stateParam, stateParamLen);
    uint8_t id;
    uint8_t index;
    index = 3;
    if (stateParam[0] == 0x05) {
        for (id = 0; id < 8; id++) { // ����ָʾ��
            if (stateParam[1] & (0x80 >> id)) {
                // printf("id %d, index %d\n", id, index);
                // plcp_panel_bk_process(id, stateParam[index]);
                index++;
            }
        }
        for (id = 0; id < 8; id++) { // ���ñ����
            if (stateParam[2] & (0x80 >> id)) {
                // printf("led id %d, index %d\n", id, index);
                // plcp_panel_led_process(id, stateParam[index]);
                index++;
            }
        }
        sycn_flag = 1;
    } else if (stateParam[0] == 0x04) {
        for (id = 0; id < 4; id++) { // ���ü̵���
            if (stateParam[1] & (0x80 >> id)) {
                // printf("relay id %d, index %d\n", id, index);
                // plcp_panel_realy_process(id, stateParam[index]);
                index++;
            }
        }
        sycn_flag = 1;
    }
    return 1;
}

/*---------------------------------------------------------------
�������ƣ�Aps_UartSendMessage
�������ܣ�APS��ͨ���û����ڷ���Uapps���ġ�
���������pMsg-�����͵�Uapps���Ľṹ��ָ��
         sendBuff-ת�������������
         sendBuffMaxSize-�����С
����ֵ��  len-���ݳ���
�� ע�����ڷ���Uapps���������Escapeǰ������
---------------------------------------------------------------*/
uint16_t Aps_UartMessage(UappsMessage *pMsg, uint8_t *sendBuff, uint16_t sendBuffMaxSize)
{
    uint16_t len               = 0;
    uint8_t escapeStr[ESC_LEN] = ESC_STRING; // ���Ĺ̶�֡ͷ

    memset(sendBuff, 0, sendBuffMaxSize * sizeof(uint8_t));
    memcpy(sendBuff, escapeStr, ESC_LEN * sizeof(uint8_t));

    len = Uapps2Bytes(pMsg, &(sendBuff[ESC_LEN]));
    len += ESC_LEN;

    return len;
}

/*--------------------------------11-----------------------------
�������ƣ�APP_SendACK
�������ܣ�APP��ack�ظ�
��������� reqMsg-Uapps�����Ľṹ�壬 payloadFlag - �ظ���ACK�Ƿ���غɣ�scratch - �ظ��غɣ�
            type-�ظ�֡����ͷ�ı������ͣ�repondCode - �ظ���
���������resMsg-���ɵ�Uapps���Ľṹ��
����ֵ��   ��
�� ע��
---------------------------------------------------------------*/
void APP_SendACK(UappsMessage *reqMsg, uint16_t payloadFlag, uapps_rw_buffer_t *scratch, uint8_t type, uint8_t repondCode)
{
    char destaddr[12];
    uint8_t uartSendBuff[100];
    uint16_t uartSendBuffLen = 0;
    uint16_t contentFormat   = UAPPS_FMT_OCTETS;
    UappsMessage respondPacket;
    memset(&respondPacket, 0, sizeof(UappsMessage));

    if (payloadFlag) {
        UappsDataResponse(reqMsg, &respondPacket, type, repondCode, scratch->p, scratch->len, contentFormat, 0); // ���غɽ��лظ�
    } else {
        UappsSimpleResponse(reqMsg, &respondPacket, type, repondCode);
    }
    UappsGetNodaFromOptionFROM(reqMsg, destaddr);
    UappsAddFromOption(&respondPacket, destaddr);
    uartSendBuffLen = Aps_UartMessage(&respondPacket, uartSendBuff, 100);
    // MCU_Send_date(uartSendBuff, uartSendBuffLen);
    app_usart_tx_buf(uartSendBuff, uartSendBuffLen, USART0);
}

/*--------------------------------29-----------------------------
�������ƣ�APP_STATE_SET_STATE
�������ܣ��豸�����մ�������״̬�ĺ���
���������uappsMsg--��Ϣָ��
����ֵ��   ��
�� ע����������ָ��uappsMsg->pl_ptr״̬���ݣ�uappsMsg->pl_len���ݳ���
---------------------------------------------------------------*/
void APP_STATE_SET_STATE(UappsMessage *uappsMsg, RSL_t *rsiMsg)
{
    uapps_rw_buffer_t scratch;
    uint8_t respondCode = UAPPS_BAD_REQUEST;
    memset(&scratch, 0, sizeof(uapps_rw_buffer_t));
    // printf("enter SET_STATE\n");

#if 0
    if (uappsMsg->hdr.hdrCode == UAPPS_REQ_PUT) {
        if (PLCP_write_state(rsiMsg->aei, uappsMsg->pl_ptr, uappsMsg->pl_len)) {
            respondCode = UAPPS_ACK_CHANGED;
        }
    }
#endif
    if (uappsMsg->hdr.hdrCode == UAPPS_REQ_PUT) {
        if (plcp_panel_set_status(rsiMsg->aei, uappsMsg->pl_ptr, uappsMsg->pl_len)) {
            respondCode = UAPPS_ACK_CHANGED;
        }
    }

    // else if(uappsMsg->hdr.hdrCode == UAPPS_REQ_GET)
    // {
    // 	respondCode = UAPPS_ACK_CONTENT;
    // 	ackLen = PlcSdkCallbackStateParamRead(rsiMsg->aei, uappsMsg->pl_ptr, uappsMsg->pl_len, ackBuf);
    // 	if(ackLen != 0)
    // 	{
    // 		payloadFlag = 1;
    // 		scratch.p = ackBuf;
    // 		scratch.len = ackLen;
    // 	}
    // }

    // �ظ�ACK
    if (uappsMsg->hdr.type == UAPPS_TYPE_CON) // �ж��յ��ı������Ƿ���Ҫ���лظ�
    {
        APP_SendACK(uappsMsg, 0, &scratch, UAPPS_TYPE_ACK, respondCode);
    }
}

#if 0
void APP_OTA_UPDATA_Processing(UappsMessage* uappsMsg)
{
	uapps_rw_buffer_t scratch;
	uint8_t respondCode = UAPPS_BAD_REQUEST;
	memset(&scratch, 0, sizeof(uapps_rw_buffer_t));

	if (uappsMsg->hdr.hdrCode == UAPPS_REQ_PUT)
	{
		if(PLCP_ota_updata(uappsMsg->pl_ptr, uappsMsg->pl_len))
		{
			respondCode = UAPPS_ACK_CONTENT;
		}
	}

	// #define error_msg		"req proc fail"

    // static char errMsg[64] = {0};
    // snprintf(errMsg, sizeof(errMsg), "I:%s, E:%d", info, code);


	// 	APP_MSE_create_error_msg(&scratch, "req proc fail", lerr);
	// 	scratch->p = error_msg;
	// 	scratch->len = strlen(error_msg);
	// 	payloadFlag = 1;
	// 	respondCode = UAPPS_SERVER_ERROR;

	if (uappsMsg->hdr.type == UAPPS_TYPE_CON)
	{
		APP_SendACK(uappsMsg, 0, &scratch, UAPPS_TYPE_ACK, respondCode);
	}
}

void APP_OTA_FILE_Processing(UappsMessage* uappsMsg)
{
	uapps_rw_buffer_t scratch;
	uint8_t respondCode = UAPPS_BAD_REQUEST;
	memset(&scratch, 0, sizeof(uapps_rw_buffer_t));

	if (uappsMsg->hdr.hdrCode == UAPPS_REQ_PUT)
	{
		if(PLCP_ota_file(uappsMsg->pl_ptr, uappsMsg->pl_len))
		{
			respondCode = UAPPS_ACK_CONTENT;
		}
	}

	if (uappsMsg->hdr.type == UAPPS_TYPE_CON)
	{
		APP_SendACK(uappsMsg, 0, &scratch, UAPPS_TYPE_ACK, respondCode);
	}
}

void APP_OTA_RES_Processing(UappsMessage* uappsMsg)
{
	uapps_rw_buffer_t scratch;
	uint8_t respondCode = UAPPS_BAD_REQUEST;
	uint8_t payloadFlag = 0;
	uint8_t ackBuf[100];
	memset(&scratch, 0, sizeof(uapps_rw_buffer_t));

	// printf("enter SCENE_LIST\n");
	if (uappsMsg->hdr.hdrCode == UAPPS_REQ_GET)
	{
		scratch.len = PLCP_ota_res(ackBuf);
		if(scratch.len != 0)
		{
			scratch.p = ackBuf;
			payloadFlag = 1;
			respondCode = UAPPS_ACK_CONTENT;
		}
	}

	//�ظ�ACK
	if (uappsMsg->hdr.type == UAPPS_TYPE_CON)//�ж��յ��ı������Ƿ���Ҫ���лظ�
	{
		APP_SendACK(uappsMsg, payloadFlag, &scratch, UAPPS_TYPE_ACK, respondCode);
	}
}
#endif
/*--------------------------------29-----------------------------
�������ƣ�APP_TOPIC_MSEProcessing
�������ܣ��ֱ�����ܵ���ָ��
���������uappsMsg�����Ľṹ�壬rsiMsg-��������ָ��
����ֵ��   ��
�� ע��
---------------------------------------------------------------*/
void APP_TOPIC_MSEProcessing(UappsMessage *uappsMsg, RSL_t *rsiMsg)
{
    if (strcmp(rsiMsg->rsi, MSE_DEVICE_SET_STATE) == 0) {
        APP_STATE_SET_STATE(uappsMsg, rsiMsg);
    }
#if 0
	else if (strcmp(rsiMsg->rsi, MSE_OTA_UPDATE) == 0) {
        APP_OTA_UPDATA_Processing(uappsMsg);
    } else if (strcmp(rsiMsg->rsi, MSE_OTA_FILE) == 0) {
        APP_OTA_FILE_Processing(uappsMsg);
    } else if (strcmp(rsiMsg->rsi, MSE_OTA_RES) == 0) {
        APP_OTA_RES_Processing(uappsMsg);
    }
#endif
}

/*--------------------------------1-----------------------------
�������ƣ�APP_Tx_ProcessOfACKReceived
�������ܣ�APP���յ��ظ�ȷ��֡�Ĵ�������
���������uappsMsg--��Ϣָ��
����ֵ��0-ack��ƥ��,1-ackƥ��
�� ע��
---------------------------------------------------------------*/
extern uint8_t button_num_max_get;
extern uint8_t relay_num_max_get;
extern uint16_t local_did;
void APP_Tx_ProcessOfACKReceived(UappsMessage *uappsMsg)
{
    if (uappsMsg->hdr.hdrCode == UAPPS_ACK_CONTENT && uappsMsg->token[0] == 0x52) {
        switch (uappsMsg->token[1]) {
            case MSE_GET_DID:
                memcpy(&local_did, uappsMsg->pl_ptr, 2 * sizeof(uint8_t));
                // printf("get DID success:%04x!\r\n",local_did);
                break;
            case MSE_GET_INIT_INFO:
                button_num_max_get = uappsMsg->pl_ptr[0];
                relay_num_max_get  = uappsMsg->pl_ptr[1];
                // printf("get init info success: %d, %d!\r\n", button_num_max_get, relay_num_max_get);
                break;
            default:
                break;
        }
    }
}

/*--------------------------------29-----------------------------
�������ƣ�APP_UappsMsgProcessing
�������ܣ����ݴ��ڽ��ܵ������ݽ��д���
���������inputDataBuff-���ݣ�inputDataBuffLen-���ݳ���
����ֵ��   ��
�� ע���ú������ڴ��ڽ������ݺ���д������
---------------------------------------------------------------*/
void APP_UappsMsgProcessing(uint8_t *inputDataBuff, uint16_t inputDataBuffLen)
{
    uint8_t isUappsMsg          = 0;
    uint16_t uappsMsgStartIndex = 0;
    uint16_t uappsMsgLen        = 0;
    uint8_t isRSIDataFlag       = 0;
    static UappsMessage uappsMsg;
    RSL_t rsiMsg;
    static uint8_t rsiStr[100];
    uint16_t optionValueLen = 0;

    memset(&uappsMsg, 0, sizeof(UappsMessage));
    memset(&rsiMsg, 0, sizeof(RSL_t));
    memset(rsiStr, 0, sizeof(rsiStr));

    isUappsMsg = isUappsData((char *)inputDataBuff, inputDataBuffLen, &uappsMsgStartIndex);
    if (1 == isUappsMsg) {
        uappsMsgLen    = inputDataBuffLen - uappsMsgStartIndex - ESC_LEN;
        isRSIDataFlag  = UappsFromBytes(&inputDataBuff[uappsMsgStartIndex + ESC_LEN], uappsMsgLen, &uappsMsg);
        optionValueLen = uappsMsg.options[0].opt_len;
        memcpy(rsiStr, uappsMsg.options[0].opt_val, optionValueLen * sizeof(uint8_t));
        rsiStr[optionValueLen] = '\0';
        RslFromStr(&rsiMsg, (char *)rsiStr);
        if (UAPPS_OK == isRSIDataFlag) {
            // �����û��Զ���Ĵ�����
            APP_TOPIC_MSEProcessing(&uappsMsg, &rsiMsg);
            // ack������
            APP_Tx_ProcessOfACKReceived(&uappsMsg);
        }
        // else
        // {
        // 	// ���ݴ��ڽ��յ������ݽ����ж�֮��ΪUappsָ����mcu��Ҫ�Ը��������ⵥ�����д�����ŵ��˴����н���
        // 	printf("not Uapps msg\r\n");
        // }
    }
    // else
    // {
    // 	printf("no Topic opt\n");
    // }
}

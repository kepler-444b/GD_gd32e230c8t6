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
 * description: 本文件是收到数据后进行解析判断是否为Uapps报文再进行分包处理参考例程
 *
 * modify record:主要进行解析Uapps报文并依据对应的功能进行处理响应
 *****************************************************************************/
#include "MseProcess.h"
#include "PLCP_ota.h"
#include "../usart/usart.h"
#include "gd32e23x.h"
#include "../base/debug.h"
#include "../device/plcp_panel.h"

#define MSE_DEVICE_SET_STATE "/_state" // 设置状态

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
        for (id = 0; id < 8; id++) { // 设置指示灯
            if (stateParam[1] & (0x80 >> id)) {
                // printf("id %d, index %d\n", id, index);
                // plcp_panel_bk_process(id, stateParam[index]);
                index++;
            }
        }
        for (id = 0; id < 8; id++) { // 设置背光灯
            if (stateParam[2] & (0x80 >> id)) {
                // printf("led id %d, index %d\n", id, index);
                // plcp_panel_led_process(id, stateParam[index]);
                index++;
            }
        }
        sycn_flag = 1;
    } else if (stateParam[0] == 0x04) {
        for (id = 0; id < 4; id++) { // 设置继电器
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
函数名称：Aps_UartSendMessage
函数功能：APS层通过用户串口发送Uapps报文。
输入参数：pMsg-待发送的Uapps报文结构体指针
         sendBuff-转化后的数据数组
         sendBuffMaxSize-数组大小
返回值：  len-数据长度
备 注：串口发送Uapps报文需加上Escape前导序列
---------------------------------------------------------------*/
uint16_t Aps_UartMessage(UappsMessage *pMsg, uint8_t *sendBuff, uint16_t sendBuffMaxSize)
{
    uint16_t len               = 0;
    uint8_t escapeStr[ESC_LEN] = ESC_STRING; // 报文固定帧头

    memset(sendBuff, 0, sendBuffMaxSize * sizeof(uint8_t));
    memcpy(sendBuff, escapeStr, ESC_LEN * sizeof(uint8_t));

    len = Uapps2Bytes(pMsg, &(sendBuff[ESC_LEN]));
    len += ESC_LEN;

    return len;
}

/*--------------------------------11-----------------------------
函数名称：APP_SendACK
函数功能：APP层ack回复
输入参数： reqMsg-Uapps请求报文结构体， payloadFlag - 回复的ACK是否带载荷，scratch - 回复载荷，
            type-回复帧报文头的报文类型，repondCode - 回复码
输出参数：resMsg-生成的Uapps报文结构体
返回值：   无
备 注：
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
        UappsDataResponse(reqMsg, &respondPacket, type, repondCode, scratch->p, scratch->len, contentFormat, 0); // 带载荷进行回复
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
函数名称：APP_STATE_SET_STATE
函数功能：设备端最终处理配置状态的函数
输入参数：uappsMsg--消息指针
返回值：   无
备 注：根据数据指针uappsMsg->pl_ptr状态数据，uappsMsg->pl_len数据长度
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

    // 回复ACK
    if (uappsMsg->hdr.type == UAPPS_TYPE_CON) // 判断收到的报文内是否需要进行回复
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

	//回复ACK
	if (uappsMsg->hdr.type == UAPPS_TYPE_CON)//判断收到的报文内是否需要进行回复
	{
		APP_SendACK(uappsMsg, payloadFlag, &scratch, UAPPS_TYPE_ACK, respondCode);
	}
}
#endif
/*--------------------------------29-----------------------------
函数名称：APP_TOPIC_MSEProcessing
函数功能：分别处理接受到的指令
输入参数：uappsMsg请求报文结构体，rsiMsg-输入数据指针
返回值：   无
备 注：
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
函数名称：APP_Tx_ProcessOfACKReceived
函数功能：APP层收到回复确认帧的处理函数。
输入参数：uappsMsg--消息指针
返回值：0-ack不匹配,1-ack匹配
备 注：
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
函数名称：APP_UappsMsgProcessing
函数功能：根据串口接受到的数据进行处理
输入参数：inputDataBuff-数据，inputDataBuffLen-数据长度
返回值：   无
备 注：该函数用于串口接收数据后进行处理解析
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
            // 调用用户自定义的处理函数
            APP_TOPIC_MSEProcessing(&uappsMsg, &rsiMsg);
            // ack处理函数
            APP_Tx_ProcessOfACKReceived(&uappsMsg);
        }
        // else
        // {
        // 	// 根据串口接收到的数据解析判断之后不为Uapps指令。如果mcu需要对该数据另外单独进行处理则放到此处进行解析
        // 	printf("not Uapps msg\r\n");
        // }
    }
    // else
    // {
    // 	printf("no Topic opt\n");
    // }
}

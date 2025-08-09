/******************************************************************************
 * (c) CRCright 2017-2018, Leaguer Microelectronics, All Rights Reserved
 * THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF Leaguer Microelectronics, INC.
 * The CRCright notice above does not evidence any actual or intended
 * publication of such source code.
 *
 * file: HostProcess.c
 * author :
 * date   : 2023/09/24
 * version: v1.0
 * description: 本文件为mcu主动组帧构造Uapps报文主动上送参考例程
 *
 * modify record:主要进行组帧构造Uapps报文并依据对应的功能进行上送
 *****************************************************************************/

#include "MseProcess.h"
#include "gd32e23x.h"
#include "../usart/usart.h"
#include "../base/debug.h"
#include "../device/device_manager.h"

#if defined PLCP_LHW

void MCU_Send_date(uint8_t *SendBuff, uint16_t SendBuffLen);

/*---------------------------------------------------------------
函数名称：Uapps_PackRSLWithFromMsg
函数功能：添加from选项
输入参数：sendBuff-发送缓存；rslStr-网关；from-本地；playload-需要上送的数据；playloadLen-数据长度
返回值：  sendBuffLen-发送缓存长度
备 注：
---------------------------------------------------------------*/

uint16_t Uapps_PackRSLWithFromMsg(uint8_t *sendBuff, char *rslStr, char *from, uint8_t *playload, uint8_t playloadLen)
{
    uint16_t sendBuffLen;
    UappsMessage txMsg;
    char RSLtemp[65];

    if (sendBuff == NULL || rslStr == NULL || strlen(rslStr) == 0) {
        return 0;
    }
    if (playloadLen > 0 && playload == NULL) {
        return 0;
    }

    strcpy(RSLtemp, rslStr);
    memset(&txMsg, 0, sizeof(UappsMessage));
    UappsCreateMessage(&txMsg, UAPPS_TYPE_NON, UAPPS_REQ_PUT, RSLtemp);

    if (from != NULL) {
        if (UAPPS_OK != UappsPutOption(&txMsg, UAPPS_OPT_FROM, (uint8_t *)from, strlen(from))) {
            return 0;
        }
    }
    if (playloadLen > 0) {
        if (UAPPS_OK != UappsPutData(&txMsg, playload, playloadLen, UAPPS_FMT_OCTETS, 0)) {
            return 0;
        }
    }

    memcpy(sendBuff, ESC_STRING, ESC_LEN);
    sendBuffLen = Uapps2Bytes(&txMsg, &sendBuff[ESC_LEN]) + ESC_LEN;
    if (sendBuffLen >= 450) {
        // printf("send data too long!\n");
        return 0;
    }

    return sendBuffLen;
}

    #include "../base/debug.h"
uint8_t APP_SendRSL(char *rslStr, char *from, uint8_t *playload, uint8_t playloadLen)
{
    uint8_t uartSendBuff[200];
    uint16_t uartSendBuffLen = 0;

    uartSendBuffLen = Uapps_PackRSLWithFromMsg(uartSendBuff, rslStr, from, playload, playloadLen);
    if (uartSendBuffLen) {
        // MCU_Send_date(uartSendBuff, uartSendBuffLen);
        app_usart_tx_buf(uartSendBuff, uartSendBuffLen, USART0);
        APP_PRINTF_BUF("PLCP_TX", uartSendBuff, uartSendBuffLen);
        return 1;
    } else {
        return 0;
    }
}

void cmd_switch_init(uint8_t button_mun, uint8_t relay_mun)
{
    uint8_t playload[2];

    playload[0] = button_mun;
    playload[1] = relay_mun;
    APP_SendRSL("@SE200./sw/_init", NULL, playload, sizeof(playload));
}

void cmd_switch_get_init_info(void)
{
    UappsMessage testCoap;
    char rsi[50] = "@SE200./sw/_init";
    uint8_t uartSendBuff[100];
    uint16_t uartSendBuffLen = 0;

    memset(&testCoap, 0, sizeof(UappsMessage));
    UappsCreateMessage(&testCoap, UAPPS_TYPE_CON, UAPPS_REQ_GET, rsi);

    // 如果应用层要设置token ID的话
    testCoap.token[0] = 0x52;
    testCoap.token[1] = MSE_GET_INIT_INFO;

    uartSendBuffLen = Aps_UartMessage(&testCoap, uartSendBuff, 100);
    // MCU_Send_date(uartSendBuff, uartSendBuffLen);
    app_usart_tx_buf(uartSendBuff, uartSendBuffLen, USART0);
}

/*--------------------------------------------------------------
函数名称：CmdTest_MSE_GET_DID
函数功能：构造获取plc模组DID的指令Uapps报文函数
输入参数：
返回值：   无
备 注：由于是本地串口直接读取plc模组的信息，因此不需要指定该mac地址。
        获取plc模组的did，did为0代表模组未入网，did不为0代表模组已入网
---------------------------------------------------------------*/
void CmdTest_MSE_GET_DID(void)
{
    UappsMessage testCoap;
    char rsi[50] = "@SE1./3/1";
    uint8_t uartSendBuff[100];
    uint16_t uartSendBuffLen = 0;

    // printf("enter GET_DID\n");

    memset(&testCoap, 0, sizeof(UappsMessage));
    UappsCreateMessage(&testCoap, UAPPS_TYPE_CON, UAPPS_REQ_GET, rsi);

    // 如果应用层要设置token ID的话
    testCoap.token[0] = 0x52;
    testCoap.token[1] = MSE_GET_DID;

    uartSendBuffLen = Aps_UartMessage(&testCoap, uartSendBuff, 100);
    // MCU_Send_date(uartSendBuff, uartSendBuffLen);
    app_usart_tx_buf(uartSendBuff, uartSendBuffLen, USART0);
    // APP_PRINTF_BUF("send", uartSendBuff, uartSendBuffLen);
}

/*--------------------------------------------------------------
函数名称：CmdTest_MSE_Factory
函数功能：构造使模组恢复出厂设置的指令Uapps报文函数
输入参数：
返回值：
备 注：由于是本地串口直接读取读取网关的信息，因此不需要指定该mac地址。
---------------------------------------------------------------*/
void CmdTest_MSE_Factory(void)
{
    UappsMessage testCoap;
    char rsi[50] = "@./0/_factory";
    uint8_t uartSendBuff[100];
    uint16_t uartSendBuffLen = 0;

    // printf("enter factory\n");

    memset(&testCoap, 0, sizeof(UappsMessage));
    UappsCreateMessage(&testCoap, UAPPS_TYPE_CON, UAPPS_REQ_PUT, rsi);

    // 如果应用层要设置token ID的话
    testCoap.token[0] = 0x52;
    testCoap.token[1] = MSE_SET_Factory;

    uartSendBuffLen = Aps_UartMessage(&testCoap, uartSendBuff, 100);
    // MCU_Send_date(uartSendBuff, uartSendBuffLen);
    app_usart_tx_buf(uartSendBuff, uartSendBuffLen, USART0);
}

void CmdTest_MSE_RST(void)
{
    UappsMessage testCoap;
    char rsi[50] = "@./0/_rst";
    uint8_t uartSendBuff[100];
    uint16_t uartSendBuffLen = 0;

    memset(&testCoap, 0, sizeof(UappsMessage));
    UappsCreateMessage(&testCoap, UAPPS_TYPE_CON, UAPPS_REQ_PUT, rsi);

    // 如果应用层要设置token ID的话
    testCoap.token[0] = 0x52;
    testCoap.token[1] = MSE_SET_RST;

    uartSendBuffLen = Aps_UartMessage(&testCoap, uartSendBuff, 100);
    // MCU_Send_date(uartSendBuff, uartSendBuffLen);
    app_usart_tx_buf(uartSendBuff, uartSendBuffLen, USART0);
}
#endif

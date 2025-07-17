/********************************************************************************
 * (c) Copyright 2017-2020, LME, All Rights Reserved。
 * THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF LME, INC。
 * The copyright notice above does not evidence any actual or intended
 * publication of such source code。
 *
 * FileName    : RSL.c
 * Author      :
 * Date        : 2020-05-09
 * Version     : 1.0
 * Description : APS层的RSL处理的源文件
 *             :
 * Others      :
 * ModifyRecord:
 *
 ********************************************************************************/

#include "Uapps.h"

/**********************************宏定义 开始*************************************/

/**********************************宏定义 结束*************************************/

/**********************************枚举声明 开始************************************/

/**********************************枚举声明 结束************************************/

/**********************************结构体声明 开始***********************************/

/**********************************结构体声明 结束***********************************/

/**********************************静态变量声明 开始**********************************/

/**********************************静态变量声明 结束**********************************/

/***********************************函数声明 开始***********************************/
void RslInit(RSL_t *pRsl)
{
	pRsl->valid = 0;
	pRsl->hasSE = 0;
	pRsl->aei[0] = 0;
	pRsl->noda[0] = 0;
	pRsl->resPath[0] = 0;
	pRsl->rsi[0] = 0;
}

/**
 * Construct RSL_t struct from string expression. Return OK or error code
 * String format: [[AEI][@port]][.noda[.gwi]]/seg1/seg2/../name
 */
/*--------------------------------------------------------------
函数名称：RslFromStr
函数功能：将RSL字符串转为RSL结构体。
输入参数：rslStr-待转换的RSL字符串
输出参数：rsl-转换后RSL结构体
返回值：  0-成功，非0-失败
备 注：    无
---------------------------------------------------------------*/
UappsError RslFromStr(RSL_t *pRsl, char *rslStr)
{

	char *p;
	char rslInputString[UAPPS_MAX_OPTLEN + 1];

	RslInit(pRsl);

	if (rslStr == NULL || strlen(rslStr) == 0)
	{
		return UAPPS_RSL_INVALID;
	}

	memcpy(rslInputString, rslStr, strlen((const char *)rslStr));
	rslInputString[strlen((const char *)rslStr)] = '\0';

	// name
	p = strchr(rslInputString, '/');
	if (p != NULL)
	{
		//	*p++ = '\0';
		strcpy(pRsl->rsi, (const char *)p);
	}

	// path
	p = strchr(rslInputString, '/');
	if (p != NULL)
	{
		//	*p++ = '\0';
		strcpy(pRsl->resPath, (const char *)p);
	}

	// noda, gwi
	p = strchr(rslInputString, '.');
	if (p != NULL)
	{
		// has noda and gwi
		*p++ = '\0';
		// noda
		strcpy(pRsl->noda, (const char *)p);
	}

	// port
	p = strchr(rslInputString, '@');
	if (p != NULL)
	{
		// has port
		*p++ = '\0';
		if ((p[0] == 'S' || p[0] == 's') && (p[1] == 'E' || p[1] == 'E'))
		{
			p += 2;
		}

		if (!isNumber(p))
		{
			return UAPPS_INVALID;
		}
		pRsl->se = (uint16_t)atol(p);
		pRsl->hasSE = 1;
	}

	// AEI
	if (strlen((const char *)rslInputString) > 0)
	{
		strcpy(pRsl->aei, (const char *)rslInputString);
	}

	pRsl->valid = 1;

	return UAPPS_OK;
}

/**
 * Convert RSL struct to normal string expression. Return Ok or error
 */
/*--------------------------------------------------------------
函数名称：Rsl2Str
函数功能：将RSL结构体转为RSL字符串。
输入参数：rsl-待转换的RSL结构体
输出参数：buf-转换后的RSL字符串
返回值：  0-成功，非0-失败
备 注：    无
---------------------------------------------------------------*/
UappsError Rsl2Str(RSL_t *pRsl, char *buf)
{
	uint8_t i;

	buf[0] = '\0';
	if (pRsl->valid == 0)
	{
		return UAPPS_RSL_INVALID;
	}

	// aei
	if (strlen((const char *)pRsl->aei) > 0)
	{
		strcat(buf, pRsl->aei);
	}

	// port
	if (pRsl->hasSE)
	{
		strcat(buf, "@");
		i = strlen((const char *)buf);
		Itoa((long)pRsl->se, buf + i);
	}

	// noda
	if (strlen((const char *)pRsl->noda) > 0)
	{
		strcat(buf, ".");
		strcat(buf, pRsl->noda);
	}

	// path
	if (strlen((const char *)pRsl->resPath) > 0)
	{
		strcat(buf, "/");
		strcat(buf, pRsl->resPath);
	}

	// name
	strcat(buf, "/");
	strcat(buf, pRsl->rsi);

	return UAPPS_OK;
}

/**
 * Do string compression to bytes, return UAPPS_OK or ERROR.
 * String format: number "123456", hex "0x1234abcd", and normal char sequence.
 * buf[0] lower 6 bits holds number of bytes
 */
UappsError RslStr2Bytes(char *s, uint8_t *buf)
{
	uint8_t type;

	buf[0] = 0;

	if (isNumber(s))
	{
		type = ENC_TYPE_BCD;
		buf[0] = str2bcd(s, strlen((const char *)s), buf + 1) & 0x3f;
	}
	else if (isHex(s))
	{
		type = ENC_TYPE_HEX;
		buf[0] = str2hex(s, strlen((const char *)s), buf + 1) & 0x3f;
	}
	else
	{
		// String
		type = ENC_TYPE_STR;
		strcpy((char *)buf + 1, (const char *)s);
		buf[0] = strlen((const char *)s) & 0x3f;
	}

	buf[0] |= (uint8_t)(type << 6);

	return UAPPS_OK;
}

/**
 * Decompress bytes into string, return string length.
 */
uint8_t RslBytes2Str(uint8_t *buf, char *s)
{
	uint8_t type, len;

	type = buf[0] >> 6;
	len = buf[0] & 0x3f;
	if (type == ENC_TYPE_BCD)
	{
		bcd2str(buf + 1, len, s);
	}
	else if (type == ENC_TYPE_HEX)
	{
		hexTostr(buf + 1, len, "0x", s);
	}
	else
	{ // String
		strncpy(s, (const char *)buf + 1, len);
	}

	return strlen(s);
}

/**
 * Convert RSL_t struct to bytes, return byte array length
 */
/*--------------------------------------------------------------
函数名称：Rsl2Bytes
函数功能：将RSL结构体转为RSL字节流。
输入参数：rsl-待转换的RSL结构体
		 enc-编码方式：0-不压缩编码，1-压缩编码
输出参数：buf-转换后的RSL字节流
返回值：  0-成功，非0-失败
备 注：    无
---------------------------------------------------------------*/
uint8_t Rsl2Bytes(RSL_t *pRsl, uint8_t *buf, uint8_t enc) // 1
{
	uint8_t i;
	char *p, *p1;
	char tmp[RES_PATH_MAXSZ + 1];

	if (enc == 0)
	{ // no compression
		if (Rsl2Str(pRsl, (char *)buf) != UAPPS_OK)
		{
			return 0;
		}
		return strlen((const char *)buf);
	}

	// Do compression encoding
	buf[0] = 0x80;
	i = 1;
	// port
	if (pRsl->hasSE)
	{
		// has port
		buf[0] |= 0x08; // port flag
		buf[i++] = (uint8_t)(pRsl->se >> 8);
		buf[i++] = (uint8_t)(pRsl->se & 0xff);
	}

	// AEI
	if (strlen((const char *)pRsl->aei) > 0)
	{
		buf[0] |= 0x04; // AEI flag
		if (RslStr2Bytes(pRsl->aei, buf + i) == UAPPS_OK)
		{
			i += (buf[i] & 0x3f) + 1;
		}
	}

	// noda
	if (strlen((const char *)pRsl->noda) > 0)
	{
		buf[0] |= 0x02; // noda flag
		if (RslStr2Bytes(pRsl->noda, buf + i) == UAPPS_OK)
			i += (buf[i] & 0x3f) + 1;
	}

	// Path
	if (strlen((const char *)pRsl->resPath) > 0)
	{
		// First, copy resPath to tmp buffer
		strcpy(tmp, (const char *)pRsl->resPath);
		p = tmp;
		while (p != NULL)
		{
			p1 = strchr(tmp, '/');
			if (p1 != NULL)
			{
				*p1++ = '\0';
			}
			if (RslStr2Bytes(p, buf + i) == UAPPS_OK)
			{
				i += (buf[i] & 0x3f) + 1;
			}
			p = p1;
		}
	}

	// Name
	if (strlen((const char *)pRsl->rsi) > 0)
	{
		if (RslStr2Bytes(pRsl->rsi, buf + i) == UAPPS_OK)
		{
			i += (buf[i] & 0x3f) + 1;
		}
	}

	return i;
}

/**
 * Construct RSL_t struct from bytes, return OK or error code
 */
/*--------------------------------------------------------------
函数名称：RslFromBytes
函数功能：将RSL字节流转为RSL结构体。
输入参数：buf-待转换的RSL字节流数组
		buf_len-RSL字节流长度
输出参数：rsl-转换后的RSL结构体
返回值：  0-成功，非0-失败
备 注：    无
---------------------------------------------------------------*/
UappsError RslFromBytes(RSL_t *pRsl, uint8_t *buf, uint8_t buf_len)
{
	uint8_t i;

	if (buf[0] < 128)
	{
		// not compressed
		buf[buf_len] = 0;
		return RslFromStr(pRsl, (char *)buf);
	}

	// Compressed
	RslInit(pRsl);
	i = 1;
	// port
	if ((buf[0] & 0x08) != 0)
	{ // has port
		pRsl->hasSE = 1;
		pRsl->se = (buf[i++] << 8);
		pRsl->se += buf[i++];
		// pRsl->port = (buf[i++] << 8) + buf[i++];
	}

	// AEI
	pRsl->aei[0] = '\0';
	if ((buf[0] & 0x04) != 0)
	{ // has AEI
		RslBytes2Str(buf + i, pRsl->aei);
		i += ((buf[i] & 0x3f) + 1);
	}

	// noda
	pRsl->noda[0] = '\0';
	if ((buf[0] & 0x02) != 0)
	{ // has noda
		RslBytes2Str(buf + i, pRsl->noda);
		i += ((buf[i] & 0x3f) + 1);
	}

	// Path and name
	while (i < buf_len)
	{
		RslBytes2Str(buf + i, pRsl->rsi);
		i += ((buf[i] & 0x3f) + 1);
		if (i < buf_len)
		{
			if (strlen((const char *)pRsl->resPath) > 0)
			{
				strcat(pRsl->resPath, "/");
			}
			strcpy(pRsl->resPath, (const char *)pRsl->rsi);
		}
	}
	pRsl->valid = 1;
	return UAPPS_OK;
}

/**
 * Convert RSL_t struct to option struct, return OK or error code
 */
/*--------------------------------------------------------------
函数名称：Rsl2Option
函数功能：将RSL结构体转换为Uapps协议的Option。
输入参数：rsl-待转换的RSL结构体
		 enc-RSL的编码方式：0-不压缩编码，1-压缩编码
		 opt_code - 选项码
输出参数：opt-转换后的Uapps协议选项结构体
返回值：  0-成功，非0-失败
备 注：    无
---------------------------------------------------------------*/
UappsError Rsl2Option(RSL_t *pRsl, uint8_t opt_code, UappsOption *opt, uint8_t enc)
{
	opt->opt_code = opt_code;
	opt->opt_len = Rsl2Bytes(pRsl, opt->opt_val, enc);

	return UAPPS_OK;
}

/**
 * Convert UappsOpton (URI_PATH) into RSL_t, return OK or error
 */
/*--------------------------------------------------------------
函数名称：RslFromOpt
函数功能：将Uapps协议的Option转为RSL结构体。
输入参数：opt-待转换的Uapps协议的Option结构体
输出参数：rsl-转换后的RSL结构体
返回值：  0-成功，非0-失败
备 注：    无
---------------------------------------------------------------*/
UappsError RslFromOpt(RSL_t *gRsl, UappsOption *opt)
{
	if (opt->opt_code != UAPPS_OPT_RSL)
	{
		return UAPPS_INVALID;
	}

	return RslFromBytes(gRsl, opt->opt_val, opt->opt_len);
}

/***************************************************************************************/

/***********************************函数声明 结束***********************************/

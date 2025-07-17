/********************************************************************************
 * (c) Copyright 2017-2020, LME, All Rights Reserved��
 * THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF LME, INC��
 * The copyright notice above does not evidence any actual or intended
 * publication of such source code��
 *
 * FileName    : LmeUtils.c
 * Author      :
 * Date        : 2020-05-09
 * Version     : 1.0
 * Description : APS��Ĺ��ߵ�Դ�ļ�
 *             :
 * Others      :
 * ModifyRecord:
 *
********************************************************************************/

#include "plcp_panel.h"

/**
 * Decode string to BCD bytes, return BCD bytes length
 */
/*--------------------------------------------------------------
�������ƣ�str2bcd
�������ܣ����ַ���תΪʮ���Ƹ�ʽ��BCD����
���������str-��ת�����ַ���
        len-�ַ�������
���������buf-ת����BCD��ʽ���ֽ�����
����ֵ��  ת�����BCD���鳤��
�� ע��
---------------------------------------------------------------*/
uint8_t str2bcd(char *str, uint8_t len, uint8_t *buf)
{
	int i;
	uint8_t sz, remainder, j;

	sz = len >> 1;
    remainder = len & 0x01;
	j = 0;
   	// תBCD��
   	for (i = 0; i < sz; i++) 
	{
    	buf[i] = ((str[j++] - '0') << 4);
		buf[i]|= (str[j++]-'0');
	}
   	// ���������������Ҫ��f
   	if (remainder > 0) 
	{
		buf[i] = ((str[j++] - '0') << 4) | 0x0f;
	}

   	return (sz+remainder);
}

/**
 * Decode BCD bytes to string, return string length
 */
/*--------------------------------------------------------------
�������ƣ�bcd2str
�������ܣ���ʮ���Ƹ�ʽ��BCD��ת��Ϊ�ַ�����
���������buf-��ת����BCD��ʽ�ֽ�����
        len-BCD��ʽ�ֽ������С
���������str-ת������ַ���
����ֵ��  ת�����ַ�������
�� ע��
---------------------------------------------------------------*/
uint8_t bcd2str(uint8_t *buf, uint8_t len, char *str)
{
	int i;
	uint8_t j, b;

	j = 0;
	for (i = 0; i < len; i++) 
	{
		b = buf[i];
		str[j++] = '0'+ (b >> 4);
		b &= 0x0f;
		if (b == 15)
			break;
		str[j++] = '0' + b;
	}

	str[j] = 0;

	return j;
}

// Decimal value of hex digit   
uint8_t hex_val(char hex)
{
	if ((hex >= '0') && (hex <= '9'))
		return (hex - '0');
	else if ((hex >= 'a') && (hex <= 'f'))
		return (hex - 'a' + 10);
	else if ((hex >= 'A') && (hex <= 'F'))
		return (hex - 'A' + 10);

	return 0;
}


/**
 * Encode hex string "0123456789abcdef" into bytes, 2 digits into one byte.
 * len must be even. 
 * The string may starts with "0x" or "0X"
 * ���ر������ֽ��� 
*/
/*--------------------------------------------------------------
�������ƣ�str2hex
�������ܣ����ַ���תΪʮ�����Ƹ�ʽ��HEX����
���������str-��ת�����ַ���
        len-�ַ�������
���������buf-ת����HEX��ʽ���ֽ�����
����ֵ��  ת�����BCD���鳤��
�� ע��    ��ת�����ַ���������"0x" or "0X"��ͷ
---------------------------------------------------------------*/
uint8_t str2hex(char *str, uint8_t len, uint8_t buf[])
{
	int i;
	uint8_t sz, j;
	char *p;

	p = str;
	if (*p == '0' && (*(p+1) == 'x' || *(p+1) == 'X'))
		p += 2;

	sz = len >> 1;
	j = 0;
   	// תHEX��
   	for (i = 0; i < sz; i++) 
	{
    	buf[i] = (hex_val(p[j++]) << 4);
		buf[i] |= hex_val(p[j++]);
	}
 	// 
   	return sz;
}

/**
 * Decode HEX in buffer to string��
 * ���ؽ����ĳ���
 */
/*--------------------------------------------------------------
�������ƣ�hex2str
�������ܣ���ʮ�����Ƹ�ʽ��HEX��תΪ�ַ�����
���������buf-��ת����HEX����
        len-HEX���鳤��
        prefix - ת������ַ���ǰ׺
���������str-ת�����ַ���
����ֵ��  ת������ַ�������
�� ע��    ��ת�����ַ���������"0x" or "0X"��ͷ
---------------------------------------------------------------*/
uint8_t hex2str(uint8_t *buf, uint8_t len, char *prefix, char *str)
{
	uint8_t i, j;
	uint8_t b;

	j = 0;
	if (prefix != NULL)
	{
		strcpy(str,prefix);
		j = strlen(prefix);
	}
		
	for (i = 0; i < len; i++) 
	{
		b = buf[i] >> 4;
		if (b <= 9)
			str[j++] = '0'+ b;
		else {
			str[j++] = 'a' + b-10;
		}

		b = buf[i] & 0x0f;
		if (b <= 9)
			str[j++] = '0'+ b;
		else {
			str[j++] = 'a' + b-10;
		}
	}

	str[j] = 0; 

	return j;
}

/*--------------------------------------------------------------
�������ƣ�isNumber
�������ܣ��ж��ַ��Ƿ�������֡�
���������s-���жϵ��ַ�
�����������
����ֵ��  1-�����֣�0-��������
�� ע��    ��
---------------------------------------------------------------*/
uint8_t isNumber(char *s)
{
	char c;
	while(*s != 0)
	{
		c = *s++;
		if (c < '0' || c > '9')
			return 0;
	} 

	return 1;
}

/*--------------------------------------------------------------
�������ƣ�isNumber
�������ܣ��ж��ַ��Ƿ����ʮ���������֡�
���������s-���жϵ��ַ�
�����������
����ֵ��  1-��ʮ���������֣�0-����ʮ����������
�� ע��    ��
---------------------------------------------------------------*/
uint8_t isHex(char *s)
{
	char *hex = "0123456789abcdefABCDEF";
	char c;

	if (*s != '0' || (*(s+1) != 'x' && *(s+1) != 'X')) 
		return 0;

	s += 2;
	while (*s != 0) 
	{ 
		c = *s++;
		if (strchr(hex,c) == NULL)
			return 0;
	}

	return 1;
}

/*--------------------------------------------------------------
�������ƣ�Itoa
�������ܣ�������ת���ַ���
���������n-��ת������
���������s-ת������ַ���
����ֵ��  ��
�� ע��    ��
---------------------------------------------------------------*/
void Itoa(long n,char s[])
{
	int i,j;
	long sign;
	char tmp[12];

	if((sign=n)<0)//��¼����
		n=-n;//ʹn��Ϊ����

	i=0;
	do {
		tmp[i++]=n%10+'0';//ȡ��һ������
	} while ((n/=10)>0);//ɾ��������

	if(sign<0)
		tmp[i++]='-';
	
	i--;
	j = 0;
	while(i >= 0)
		s[j++] = tmp[i--];

	s[j] = '\0';

}


#define isUpper(x) (x>='A' && x<='Z') //�ж��Ǵ�д�ַ���
#define isLower(x) (x>='a' && x<='z') //�ж���Сд�ַ���
#define toLowerCase(x) (x-'A'+'a')    //תΪСд
#define toUpperCase(x) (x-'a'+'A')    //תΪ��д

/*--------------------------------------------------------------
�������ƣ�toLower
�������ܣ�תΪСд��ĸ��
���������s-��ת������ĸ
���������s-ת�����Сд��ĸ
����ֵ��  ��
�� ע��    ��
---------------------------------------------------------------*/
void toLower(char *s)
{
	int i;

	if (s[0] == 0)
	{
		return;
	}

	for(i = 0; s[i]; i++)
	{
		if (isUpper(s[i])) 
		{
			s[i] = toLowerCase(s[i]); //����Ǵ�д�ַ���תΪСд��
		}
	}
}

/*--------------------------------------------------------------
�������ƣ�toUpper
�������ܣ�תΪ��д��ĸ��
���������s-��ת������ĸ
���������s-ת����Ĵ�д��ĸ
����ֵ��  ��
�� ע��    ��
---------------------------------------------------------------*/
void toUpper(char *s)
{
	int i;

	if (s[0] == 0)
	{
		return;
	}

	for(i = 0; s[i]; i++)
	{
		if (isLower(s[i])) 
		{
			s[i] = toUpperCase(s[i]); 
		}
	}
}









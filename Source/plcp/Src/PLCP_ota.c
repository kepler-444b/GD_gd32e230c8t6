
#if 0
#include "plcp_panel.h"
#include "cJSON.h"
#include "string.h"

#include "APP_timer.h"


/***************************************************************************************/
    #define OTA_MCU_FW_VER             "4015D_MCU_V1.00"
/***************************************************************************************/

typedef struct{
    uint8_t magic[2];
    uint8_t crcCheckSum[2];
    uint32_t blockNum;
    uint16_t len;
    uint8_t pad[2];
}OTA_file_block_head_S;

    #define OTA_REQUEST_VESION_STR_OLD "oldVer"
    #define OTA_REQUEST_SIZE           "size"
    #define OTA_REQUEST_BLOCK_SIZE     "blkSize"

    #define OTA_TOTAL_SIZE_MAX         48 * 1024      

static uint8_t ota_state = 0;
static uint32_t ota_total_size = 0;
static uint32_t ota_block_size = 0;

static uint32_t last_block_num = 0;
static uint32_t file_offset = 0;

static uint8_t ota_timer = 0xff;
    #define ota_timeout                30000 // 30sec

uint8_t PlcSdkCallbackWriteOtaFile(uint8_t* file, uint16_t len);
void PlcSdkCallbackOtaFinish(void);
void PlcSdkCallbackOtaStop(void);

/***************************************************************************************/
static const uint16_t sCrc16InitTable[256] =
	{
		0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
		0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
		0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
		0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
		0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
		0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
		0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
		0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
		0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
		0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
		0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
		0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
		0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
		0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
		0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
		0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
		0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
		0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
		0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
		0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
		0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
		0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
		0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
		0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
		0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
		0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
		0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
		0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
		0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
		0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
		0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
		0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0};

uint16_t CRC16_Ccitt1021_Cal(uint16_t scrc, uint8_t *pData, uint16_t len)
{
	uint16_t crc;
	uint8_t da;

	crc = scrc;

	while (len-- != 0)
	{
		da = (uint8_t)(crc / 256);
		crc <<= 8;
		crc ^= sCrc16InitTable[da ^ *pData];
		pData++;
	}

	return (crc);
}

/***************************************************************************************/
static void ota_stop(void)
{
	ota_state = 0;
	ota_total_size = 0;
	ota_block_size = 0;

	last_block_num = 0;
	file_offset = 0;

	APP_StopGenTimer(ota_timer);
	PlcSdkCallbackOtaStop();
}

uint8_t PLCP_ota_updata(uint8_t* updata_msg, uint16_t msg_len)
{
    cJSON * root = NULL, *temp = NULL;
	uint8_t ret = 0;

	if(ota_state){
		goto ota_updata_return;
	}

	root = cJSON_ParseWithLength((char *)updata_msg, msg_len);
	if (root == NULL)
	{
		goto ota_updata_return;
	}

	temp = cJSON_GetObjectItem(root, OTA_REQUEST_SIZE);
	if(temp == NULL){
		goto ota_updata_return;
	}
	if(!cJSON_IsNumber(temp)){
		goto ota_updata_return;
	}
	ota_total_size = cJSON_GetNumberValue(temp);
	if(ota_total_size > OTA_TOTAL_SIZE_MAX || ota_total_size == 0){
		ota_total_size = 0;
		goto ota_updata_return;
	}

	temp = cJSON_GetObjectItem(root, OTA_REQUEST_VESION_STR_OLD);
	if(temp == NULL){
		goto ota_updata_return;
	}
	if(!cJSON_IsString(temp)){
		goto ota_updata_return;
	}
	// printf("old_ver %s\n", cJSON_GetStringValue(temp));
	// if(memcmp(old_ver, cJSON_GetStringValue(temp))){
	// 	return 0;
	// }

	temp = cJSON_GetObjectItem(root, OTA_REQUEST_BLOCK_SIZE);
	if(temp == NULL){
		goto ota_updata_return;
	}
	if(!cJSON_IsNumber(temp)){
		goto ota_updata_return;
	}
	ota_block_size = cJSON_GetNumberValue(temp);
	if(ota_block_size == 0){
		goto ota_updata_return;
	}

	ret = 1;
	ota_state = 1;
	// printf("ota_total_size %d, ota_block_size %d\n", ota_total_size, ota_block_size);

	if(ota_timer == 0xff){
		ota_timer = APP_NewGenTimer(ota_timeout, ota_stop);
	}
	APP_StopGenTimer(ota_timer);
	APP_StartGenTimer(ota_timer);

ota_updata_return:
	cJSON_Delete(root);
	return ret;
}


uint8_t PLCP_ota_file(uint8_t* file_msg, uint16_t msg_len)
{
    uint16_t checksum = 0;
	uint16_t file_block_len;
	uint32_t this_block_num;

    OTA_file_block_head_S *head = NULL;

    #define checkSumStart              (sizeof(head->magic) + sizeof(head->crcCheckSum))

	if(!ota_state){
		return 0;
	}

	head = (OTA_file_block_head_S *)file_msg;
	file_block_len = ((uint16_t)((((uint16_t)(head->len & 0xff00)) >> 8) | \
							((uint16_t)( head->len & 0x00ff) << 8)));
	this_block_num = (uint32_t)((((uint32_t )(head->blockNum & 0xff000000) >> 24) | \
                            (((uint32_t)(head->blockNum & 0x00ff0000)) >> 8) | \
                            (((uint32_t)(head->blockNum & 0x0000ff00)) << 8) | \
                            (((uint32_t)(head->blockNum & 0x000000ff)) << 24)));
    checksum = CRC16_Ccitt1021_Cal(0, file_msg + checkSumStart, file_block_len + sizeof(OTA_file_block_head_S) - checkSumStart);
	// printf("block:%d: len:%d crc:%04x <-> %02x %02x offset:%d\n",  this_block_num, file_block_len, checksum, head->crcCheckSum[0], head->crcCheckSum[1], file_offset);
	if(((checksum>>8)&0x00ff) != head->crcCheckSum[0] || (checksum&0x00ff) != head->crcCheckSum[1]){
		return 0;
	}
	if(this_block_num != 0 && this_block_num == last_block_num){
		return 1;
	}
	if(this_block_num != 0 && this_block_num != last_block_num+1){
		return 0;
	}
	if(file_block_len > ota_block_size){
		return 0;
	}
	if(file_offset + file_block_len > ota_total_size){
		return 0;
	}
	if(!PlcSdkCallbackWriteOtaFile(file_msg + sizeof(OTA_file_block_head_S), file_block_len)){
		return 0;
	}

	file_offset += file_block_len;
	last_block_num = this_block_num;

	APP_StopGenTimer(ota_timer);
	APP_StartGenTimer(ota_timer);

	if(file_offset == ota_total_size){
		PlcSdkCallbackOtaFinish();
		ota_stop();
	}

	return 1;
}

uint16_t PLCP_ota_res(uint8_t* ack)
{
	//"{\"res\":%s,\"mac\":%s,\"ver\":%s,\"date\":%sT%s}", res, selfMacAddr, APP_get_firmware_version(verbuf, sizeof(verbuf)), __DATE__, __TIME__);
	return sprintf((char*)ack, "{\"ver\":%s,\"date\":%s T%s}", OTA_MCU_FW_VER, __DATE__, __TIME__);
}
#endif

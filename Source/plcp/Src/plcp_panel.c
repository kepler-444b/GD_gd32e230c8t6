
#include "APP_Timer.h"
#include "plcp_panel_api.h"


void APP_Postflag_Recover(void);

void APP_Queue_ListenAndHandleMessage(void);

void APP_TxBuffer_init(void);
void APP_RxBuffer_init(void);

uint8_t MCU_UartReceive(uint8_t *recbuf, uint16_t reclen);

void CmdTest_MSE_GET_DID(void);
void CmdTest_MSE_Factory(void);
void CmdTest_MSE_RST(void);
void cmd_switch_init(uint8_t button_mun, uint8_t relay_mun);
void cmd_switch_get_init_info(void);

uint16_t local_did;

uint8_t button_num_max_set = 0xff;
uint8_t relay_num_max_set = 0xff;

uint8_t button_num_max_get = 0xff;
uint8_t relay_num_max_get = 0xff;

uint8_t sycn_flag;
uint8_t init_timer = 0xff;
uint8_t sync_timer = 0xff;

// void printData(uint8_t *d, uint16_t len)
// {
// 	printf("------\n");
// 	for(; len > 0; len--){
// 		printf("%02x ", *d);
// 		d++;
// 	}
// 	printf("\n");
// 	printf("======\n");
// }

static void init_timer_handler(void)
{
	static uint8_t step = 0;

	switch (step)
	{
	case 0:
		CmdTest_MSE_GET_DID();
		step++;
		break;

	case 1:
		if(button_num_max_get == 0xff || relay_num_max_get == 0xff){
			cmd_switch_get_init_info();
			break;
		}

		if(button_num_max_set != 0xff && relay_num_max_set != 0xff){
			if(button_num_max_set != button_num_max_get || relay_num_max_set != relay_num_max_get){
				cmd_switch_init(button_num_max_set, relay_num_max_set);
				button_num_max_get = 0xff;
				relay_num_max_get = 0xff;
				break;
			}
		}
		
		step = 0;
		break;

	default:
		step = 0;
		break;
	}
}

static void sync_timer_handler(void)
{
	static uint32_t time = 3000;

	APP_StopGenTimer(sync_timer);

	if(!sycn_flag){
		CmdTest_MSE_RST();
		time += 3000;
		APP_SetGenTimer(sync_timer, time);
		APP_StartGenTimer(sync_timer);	
	}
}

void plcp_panel_init(uint8_t button_num_max, uint8_t relay_num_max)
{
	// printf("plcp_panel_init\n");
	local_did = 0x0000;

	button_num_max_set = button_num_max;
	relay_num_max_set = relay_num_max;

	APP_RxBuffer_init();
	APP_TxBuffer_init();

	APP_Timer_Init();

	init_timer = APP_NewGenTimer(1000, init_timer_handler);
	APP_StartGenTimer(init_timer);

	sync_timer = APP_NewGenTimer(3000, sync_timer_handler);
	APP_StartGenTimer(sync_timer);	
}

void plcp_panel_main_loop_process(void)
{
	APP_Queue_ListenAndHandleMessage();
	APP_TimerRun();
}

uint8_t plcp_panel_uart_recv_process(uint8_t *recbuf, uint16_t reclen)
{
	// printf("sdk uart rec\n");
	// printData(recbuf, reclen);
	return MCU_UartReceive(recbuf, reclen);
}

void plcp_panel_factory(void)
{
	CmdTest_MSE_Factory();
}

void plcp_panel_reset(void)
{
	CmdTest_MSE_RST();
}

uint16_t plcp_panel_get_did(void)
{
	return local_did;
}

uint8_t APP_SendRSL(char *rslStr, char *from, uint8_t *playload, uint8_t playloadLen);
void plcp_panel_event_notify(uint8_t button_id)
{
	uint8_t playload[128];

	memset(playload, 0, sizeof(playload));
	snprintf((char*)playload, sizeof(playload), "{\"button\":\"k%d\"}", button_id+1);

	// strcpy((char*)playload, "{\"id\":\"_open\",\"name\":\"OPEN\",\"data\":\"1\",\"type\":\"switch\"}");
	// printf("%s\n", (char*)playload);

	APP_SendRSL("@SE200./0/_e", NULL, playload, strlen((char*)playload));
}



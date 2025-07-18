

#include "plcp_panel_api.h"

void led_ctrl(uint8_t id, uint8_t on_off);
void relay_ctrl(uint8_t id, uint8_t connect);
void bk_ctrl(uint8_t id, uint8_t on_off);
void mcu_reset(void);
void uart_send(uint8_t *send_data, uint16_t len);
void blocking_delay_ms(uint16_t ms);

uint8_t realy_state[8];
uint8_t led_state[8];
uint8_t bk_state[8];

/****************************************************************/
/****************************************************************/
void plcp_panel_led_process(uint8_t id, uint8_t onoff)
{
    led_state[id] = onoff;
    // led_ctrl(id, onoff);
}

void plcp_panel_bk_process(uint8_t id, uint8_t onoff)
{
    bk_state[id] = onoff;
    // bk_ctrl(id, onoff);
}

void plcp_panel_realy_process(uint8_t id, uint8_t onoff)
{
    realy_state[id] = onoff;
    // relay_ctrl(id, onoff);
}

void plcp_panel_uart_send_process(uint8_t *send_data, uint16_t len)
{
    // uart_send(send_data, len);
}

#if 0

/****************************************************************/
/****************************************************************/
    #include "LTM32F103CB_ota.h"
static uint8_t ota_buff[1024];
static uint16_t ota_buff_count = 0;
static uint8_t write_page = 0;
uint8_t PlcSdkCallbackWriteOtaFile(uint8_t* file, uint16_t len)
{
	uint16_t buff_free_count;
	uint16_t copy_len;

	while(len){
		buff_free_count = 1024 - ota_buff_count;
		if(buff_free_count >= len){
			copy_len = len;
		}
		else{
			copy_len = buff_free_count;
		}

		memcpy(ota_buff+ota_buff_count, file, copy_len);
		// printf("copy @ %d len %d\n", ota_buff_count, copy_len);

		ota_buff_count += copy_len;
		file += copy_len;
		len -= copy_len;

		if(ota_buff_count == 1024){
			// printf("ota write file page %d\n", write_page);
			LTM32F103CB_ota_write_page(write_page, ota_buff);
			memset(ota_buff, 0xff, 1024);
			write_page++;
			ota_buff_count = 0;
		}
	}

	return 1;
}

void PlcSdkCallbackOtaFinish(void)
{
	// printf("ota finish\n");

	if(ota_buff_count > 0){
		// printf("ota write file page %d\n", write_page);
		LTM32F103CB_ota_write_page(write_page, ota_buff);
	}

	LTM32F103CB_ota_finish();

	mcu_reset();
}

void PlcSdkCallbackOtaStop(void)
{
	// printf("ota stop\n");

	memset(ota_buff, 0xff, 1024);
	ota_buff_count = 0;
	write_page = 0;
}
#endif
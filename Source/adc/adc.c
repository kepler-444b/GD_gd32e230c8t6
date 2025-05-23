#include <stdio.h>
#include "adc.h"
#include "gd32e23x.h"
#include "../base/debug.h"
#include "../timer/timer.h"

uint8_t rcu_config(void);
uint8_t adc_config(uint8_t adc_channel);
static inline void app_get_value(void *arg);

static adc_voltage_callback_t voltage_callback = NULL;
void app_adc_callback(adc_voltage_callback_t callback)
{
    voltage_callback = callback;
}
uint8_t app_adc_init(uint8_t adc_channel)
{
    if (rcu_config() == true && adc_config(adc_channel) == true) {
        APP_PRINTF("app_adc_init\n");
        if (app_timer_start(1, app_get_value, true, NULL, "adc_timer") != TIMER_ERR_SUCCESS) {
            APP_ERROR("app_adc_init");
        }
        return true;
    } else {
        APP_PRINTF("ADC[%d] init failed\r\n", adc_channel);
        return false;
    }
}

uint8_t rcu_config(void)
{
    /* enable ADC clock */
    rcu_periph_clock_enable(RCU_ADC);
    /* config ADC clock */
    rcu_adc_clock_config(RCU_ADCCK_APB2_DIV6);

    return true;
}

uint8_t adc_config(uint8_t adc_channel)
{
    // 设置常规通道组或插入通道组通道长度
    adc_channel_length_config(ADC_INSERTED_CHANNEL, adc_channel);

    switch (adc_channel) {
        case 1:
            adc_inserted_channel_config(0U, ADC_CHANNEL_0, ADC_SAMPLETIME_239POINT5);
            break;
        case 2:
            adc_inserted_channel_config(0U, ADC_CHANNEL_0, ADC_SAMPLETIME_239POINT5);
            adc_inserted_channel_config(1U, ADC_CHANNEL_1, ADC_SAMPLETIME_239POINT5);
            break;
        case 3:
            adc_inserted_channel_config(0U, ADC_CHANNEL_0, ADC_SAMPLETIME_239POINT5);
            adc_inserted_channel_config(1U, ADC_CHANNEL_1, ADC_SAMPLETIME_239POINT5);
            adc_inserted_channel_config(2U, ADC_CHANNEL_2, ADC_SAMPLETIME_239POINT5);
            break;
        /*
        case 4:
            可在此继续添加
        */
        default:
            break;
    }
    adc_external_trigger_source_config(ADC_INSERTED_CHANNEL, ADC_EXTTRIG_INSERTED_NONE);
    adc_data_alignment_config(ADC_DATAALIGN_RIGHT);
    adc_special_function_config(ADC_SCAN_MODE, ENABLE);
    adc_external_trigger_config(ADC_INSERTED_CHANNEL, ENABLE);
    adc_enable();

    adc_calibration_enable();
    return true;
}

static inline void app_get_value(void *arg)
{
    adc_software_trigger_enable(ADC_INSERTED_CHANNEL);

    float vref_value = (ADC_IDATA0 * 3.3 / 4096);

    if (voltage_callback != NULL) // 电压值回调到子设备中处理
    {
        voltage_callback(vref_value);
    }
}

#ifndef _ADC_H_
#define _ADC_H_
#include <stdint.h>

/// @brief 配置多个 ADC 通道 最多支持 4 个通道
/// @param adc_channel  adc_channel = 1 即 ADC_IN0;adc_channel = 2 即 同时检测 ADC_IN0 和 ADC_IN1 以此类推
/// @return success:TRUE; fail:FALSE

uint8_t app_adc_init(uint8_t adc_channel);

typedef void (*adc_voltage_callback_t)(uint16_t voltage);

void app_adc_callback(adc_voltage_callback_t callback);
#endif
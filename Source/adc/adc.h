#ifndef _ADC_H_
#define _ADC_H_
#include <stdint.h>
#include <stdbool.h>


typedef struct {
    uint16_t voltage; // 原始电压值
} adc_value_t;

// 初始化函数
bool app_adc_init(uint8_t adc_channel);

// 新增的获取ADC值的函数
bool app_adc_get_value(adc_value_t *value, TickType_t timeout);

#endif
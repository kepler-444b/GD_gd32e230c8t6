#ifndef _ADC_H_
#define _ADC_H_
#include <stdint.h>
#include <stdbool.h>

/*
    2025.5.26 舒东升
    adc采集电流,支持四路adc通道,
    采用DMA传输采集的电压值
*/

// 用于配置adc通道的结构体
typedef struct {
    bool adc_channel_0;
    bool adc_channel_1;
    bool adc_channel_2;
    bool adc_channel_3;
} adc_channel_t;

// 初始化函数
void app_adc_init(adc_channel_t *adc_channel);

// 获取ADC值的函数
uint16_t *app_get_adc_value(void);

#endif
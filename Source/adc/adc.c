#include <stdio.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "timers.h"
#include "adc.h"
#include "gd32e23x.h"
#include "../base/debug.h"

// 函数声明
static void rcu_config(void);
static void dma_config(adc_channel_t *channel_num);
static void adc_config(adc_channel_t *channel_num);

static uint16_t adc_value[4] = {0};
void app_adc_init(adc_channel_t *adc_channel)
{
    rcu_config();
    dma_config(adc_channel);
    adc_config(adc_channel);
}

static void rcu_config(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_ADC);
    rcu_periph_clock_enable(RCU_DMA);
    rcu_adc_clock_config(RCU_ADCCK_APB2_DIV6);
}
static void dma_config(adc_channel_t *channel_num)
{
    // 计算实际启用的通道数量
    uint8_t active_channels = channel_num->adc_channel_0 +
                              channel_num->adc_channel_1 +
                              channel_num->adc_channel_2 +
                              channel_num->adc_channel_3;

    dma_parameter_struct dma_data_parameter;
    dma_deinit(DMA_CH0);

    dma_data_parameter.periph_addr  = (uint32_t)(&ADC_RDATA);
    dma_data_parameter.periph_inc   = DMA_PERIPH_INCREASE_DISABLE;
    dma_data_parameter.memory_addr  = (uint32_t)(&adc_value);
    dma_data_parameter.memory_inc   = DMA_MEMORY_INCREASE_ENABLE;
    dma_data_parameter.periph_width = DMA_PERIPHERAL_WIDTH_16BIT;
    dma_data_parameter.memory_width = DMA_MEMORY_WIDTH_16BIT;
    dma_data_parameter.direction    = DMA_PERIPHERAL_TO_MEMORY;
    dma_data_parameter.number       = active_channels; // 使用实际通道数量
    dma_data_parameter.priority     = DMA_PRIORITY_HIGH;

    dma_init(DMA_CH0, &dma_data_parameter);

    dma_circulation_enable(DMA_CH0);
    dma_channel_enable(DMA_CH0);
}
uint16_t *app_get_adc_value(void)
{
    return adc_value;
}

static void adc_config(adc_channel_t *channel_num)
{
    adc_special_function_config(ADC_CONTINUOUS_MODE, ENABLE); // 连续模式
    adc_special_function_config(ADC_SCAN_MODE, ENABLE);       // 扫描模式
    adc_data_alignment_config(ADC_DATAALIGN_RIGHT);           // 右对齐

    uint8_t active_channels = channel_num->adc_channel_0 +
                              channel_num->adc_channel_1 +
                              channel_num->adc_channel_2 +
                              channel_num->adc_channel_3;
    // APP_PRINTF("active_channels:%d\n", active_channels);
    adc_channel_length_config(ADC_REGULAR_CHANNEL, active_channels);
    if (channel_num->adc_channel_0) {
        adc_regular_channel_config(0, ADC_CHANNEL_0, ADC_SAMPLETIME_55POINT5);
    }
    if (channel_num->adc_channel_1) {
        adc_regular_channel_config(1, ADC_CHANNEL_1, ADC_SAMPLETIME_55POINT5);
    }
    if (channel_num->adc_channel_2) {
        adc_regular_channel_config(2, ADC_CHANNEL_2, ADC_SAMPLETIME_55POINT5);
    }
    if (channel_num->adc_channel_3) {
        adc_regular_channel_config(3, ADC_CHANNEL_3, ADC_SAMPLETIME_55POINT5);
    }

    adc_external_trigger_source_config(ADC_REGULAR_CHANNEL, ADC_EXTTRIG_REGULAR_NONE);
    adc_external_trigger_config(ADC_REGULAR_CHANNEL, ENABLE);

    adc_enable();

    adc_dma_mode_enable();
    adc_software_trigger_enable(ADC_REGULAR_CHANNEL);
}

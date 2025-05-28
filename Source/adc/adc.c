#include <stdio.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "timers.h"
#include "adc.h"
#include "gd32e23x.h"
#include "../base/debug.h"
#include "../timer/timer.h"

void rcu_config(void);
void dma_config(void);
void adc_config(void);
void app_adc_cb(TimerHandle_t xTimer);


static adc_value_t dma_buffer;
static QueueHandle_t adc_queue_handle = NULL;
bool app_adc_init(uint8_t adc_channel)
{
    rcu_config();

    adc_config();

    // 创建静态队列
    static StaticQueue_t adc_queue_control;
    static adc_value_t adc_queue_static[5]; // 直接使用结构体数组作为存储

    // 创建静态队列
    adc_queue_handle = xQueueCreateStatic(5, sizeof(adc_value_t), (uint8_t *)adc_queue_static, &adc_queue_control);
    if (adc_queue_handle == NULL) {
        return false;
    }
    dma_config();
    return true;
}

void rcu_config(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_ADC);
    rcu_periph_clock_enable(RCU_DMA);
    rcu_adc_clock_config(RCU_ADCCK_APB2_DIV6);
}
void dma_config(void)
{
    dma_parameter_struct dma_data_parameter;

    dma_deinit(DMA_CH0);

    dma_data_parameter.periph_addr  = (uint32_t)(&ADC_RDATA);
    dma_data_parameter.periph_inc   = DMA_PERIPH_INCREASE_DISABLE;
    dma_data_parameter.memory_addr  = (uint32_t)&dma_buffer.voltage;
    dma_data_parameter.memory_inc   = DMA_MEMORY_INCREASE_DISABLE;
    dma_data_parameter.periph_width = DMA_PERIPHERAL_WIDTH_16BIT;
    dma_data_parameter.memory_width = DMA_MEMORY_WIDTH_16BIT;
    dma_data_parameter.direction    = DMA_PERIPHERAL_TO_MEMORY;
    dma_data_parameter.number       = 1U;
    dma_data_parameter.priority     = DMA_PRIORITY_HIGH;
    dma_init(DMA_CH0, &dma_data_parameter);

    // 启用DMA传输完成中断
    dma_interrupt_enable(DMA_CH0, DMA_INT_FTF);
    
    // 设置中断优先级并启用中断
    nvic_irq_enable(DMA_Channel0_IRQn, 3);
    
    dma_circulation_enable(DMA_CH0);
    dma_channel_enable(DMA_CH0);
}

void DMA_Channel0_IRQHandler(void)
{
    if(dma_interrupt_flag_get(DMA_CH0, DMA_INT_FLAG_FTF)) {
        dma_interrupt_flag_clear(DMA_CH0, DMA_INT_FLAG_FTF);
        
        // 将DMA缓冲区中的数据入队
        if(adc_queue_handle != NULL) {
            BaseType_t xHigherPriorityTaskWoken = false;
            
            // 发送数据到队列(从中断上下文)
            xQueueSendFromISR(adc_queue_handle, &dma_buffer, &xHigherPriorityTaskWoken);
            
            // 如果需要的话,触发上下文切换
            // portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}
void adc_config(void)
{
    adc_special_function_config(ADC_CONTINUOUS_MODE, ENABLE);
    adc_special_function_config(ADC_SCAN_MODE, DISABLE);        // 禁用扫描模式

    adc_data_alignment_config(ADC_DATAALIGN_RIGHT);
    adc_channel_length_config(ADC_REGULAR_CHANNEL, 1U);         // 只使用1个通道

    adc_regular_channel_config(0, 0, ADC_SAMPLETIME_55POINT5);  // 配置通道

    adc_external_trigger_source_config(ADC_REGULAR_CHANNEL, ADC_EXTTRIG_REGULAR_NONE);
    adc_external_trigger_config(ADC_REGULAR_CHANNEL, ENABLE);

    adc_enable();
    adc_calibration_enable();
    adc_dma_mode_enable();
    adc_software_trigger_enable(ADC_REGULAR_CHANNEL);
}

bool app_adc_get_value(adc_value_t *value, TickType_t timeout) {
    // 检查参数有效性
    if (adc_queue_handle == NULL || value == NULL) {
        APP_ERROR("Invalid parameters or ADC not initialized\n");
        return false;
    }
    
    // 从队列获取数据
    if (xQueueReceive(adc_queue_handle, value, timeout) != pdTRUE) {
        APP_PRINTF("Failed to get ADC value from queue\n");
        return false;
    }
    
    return true;
}

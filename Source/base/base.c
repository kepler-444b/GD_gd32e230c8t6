#include "gd32e23x.h"
#include "base.h"
#include <stdio.h>
#include <float.h>

dev_config_t my_dev_config = {0};

typedef struct
{
    uint8_t pwm_counter;
    uint8_t pwm_duty;
    uint8_t dither_accumulator; // 抖动累加器
    uint8_t pwm_cycle;          // PWM 周期
    gpio_pin_typedef_t gpio;    // 输出的管脚
} pwm_state;
static pwm_state my_pwm_state;

float app_calculate_average(const uint16_t *buffer, uint16_t count)
{
    // 参数检查
    if (buffer == NULL || count == 0) {
        return FLT_MAX; // 或返回 0.0f，根据需求决定
    }

    float sum = 0.0f;
    for (uint16_t i = 0; i < count; i++) {
        sum += buffer[i];
    }

    // 避免除以零（虽然前面已检查 count>0）
    return (count != 0) ? (sum / (float)count) : FLT_MAX;
}


bool app_uint8_to_uint32(const uint8_t *input, size_t input_count, uint32_t *output, size_t output_count)
{
    if (!input || !output) return false;
    if (output_count < (input_count + 3) / 4) {
        return false;
    }

    for (size_t i = 0; i < (input_count + 3) / 4; i++) {
        uint32_t packed      = 0;
        size_t bytes_to_pack = (input_count - i * 4) >= 4 ? 4 : (input_count - i * 4);

        for (size_t j = 0; j < bytes_to_pack; j++) {
            packed |= (uint32_t)input[i * 4 + j] << (j * 8); // 小端序
        }
        output[i] = packed;
    }
    return true;
}

bool app_uint32_to_uint8(const uint32_t *input, size_t input_count, uint8_t *output, size_t output_count)
{
    if (!input || !output) return false;
    if (output_count < input_count * 4) {
        return false;
    }

    for (size_t i = 0; i < input_count; i++) {
        for (size_t j = 0; j < 4; j++) {
            output[i * 4 + j] = (uint8_t)((input[i] >> (j * 8)) & 0xFF); // 小端序
        }
    }
    return true;
}


